# python_motor_controller.py
# Streamlit UI for motor controller with test mode configurations

import subprocess
import os
import sys
import json
import time
import pandas as pd
import streamlit as st
import Wheeljack_metadata as wheeljack_metadata
from enum import Enum
from vpic import Client


# ----- CLASS DEFINITIONS

class Mode(Enum):
    NORMAL = 1
    POWER_SUPPLY_TEST = 2
    MOTOR_CONTROL_TEST = 3
    FULL_TEST = 4


class MotorController:
    def __init__(self, executable='wheeljack.exe', test_mode=Mode.NORMAL):
        self.executable = executable
        self.test_mode = test_mode

        if not os.path.exists(self.executable):
            raise FileNotFoundError(f'Executable not found: {self.executable}')

        self.init_messages = [
            f'/ {self.executable}',
            f'Mode: {self._get_test_mode_description()}'
        ]

    def _get_test_mode_description(self):
        descriptions = {
            Mode.NORMAL: 'Normal mode (both devices required)',
            Mode.POWER_SUPPLY_TEST: 'Power supply test (no Rigol required)',
            Mode.MOTOR_CONTROL_TEST: 'Motor control test (no relay board required)',
            Mode.FULL_TEST: 'Full test mode (no hardware required)'
        }
        return descriptions.get(self.test_mode, 'Unknown mode')

    def set_test_mode(self, test_mode):
        self.test_mode = test_mode
        st.info(f'Test mode changed to: {self._get_test_mode_description()}')

    def run_motor(self, pattern, voltage, current, runtime, reverse=False):
        if isinstance(pattern, int):
            pattern_str = format(pattern, '04b')
        else:
            pattern_str = pattern

        direction_char = 'r' if reverse else 'f'
        input_data = f'{self.test_mode.value}\n{pattern_str}\n{direction_char}\n{voltage}\n{current}\n{runtime}\n'

        try:
            process = subprocess.Popen(
                [self.executable],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                universal_newlines=True
            )

            process.stdin.write(input_data)
            process.stdin.flush()
            process.stdin.close()

            output_lines = []
            last_voltage = last_current = last_power = None

            while True:
                line = process.stdout.readline()
                if not line:
                    break
                output_lines.append(line.rstrip())
                print(line.rstrip())

                if '[Sample' in line and 'V:' in line and 'I:' in line and 'P:' in line:
                    try:
                        parts = line.split()
                        for i, part in enumerate(parts):
                            if part == 'V:':
                                last_voltage = float(parts[i+1].rstrip('V'))
                            elif part == 'I:':
                                last_current = float(parts[i+1].rstrip('A'))
                            elif part == 'P:':
                                last_power = float(parts[i+1].rstrip('W'))
                    except (ValueError, IndexError):
                        pass

            return_code = process.wait()

            if return_code != 0:
                stderr = process.stderr.read()
                if stderr:
                    st.error(f'Error output: {stderr}')

            return return_code, last_voltage, last_current, last_power

        except Exception as e:
            st.error(f'Error running executable: {e}')
            return 1, None, None, None

    def run_sequence(self, motor_configs):
        if isinstance(motor_configs, list):
            motor_configs = pd.DataFrame(motor_configs)

        if 'motor_id' not in motor_configs.columns:
            motor_configs['motor_id'] = [f'Motor_{k+1}' for k in range(len(motor_configs))]

        if 'reverse' not in motor_configs.columns:
            motor_configs['reverse'] = False

        results = []
        result_cols = st.columns(4)
        motor_col_map = {'Motor_0': 0, 'Motor_1': 1, 'Motor_2': 2, 'Motor_3': 3, 'ALL': 3}

        for k, row in motor_configs.iterrows():
            if 'stop_requested' in st.session_state and st.session_state.stop_requested:
                break

            motor_id = row['motor_id']
            pattern = row['pattern']
            reverse = row.get('reverse', False)

            return_code, voltage, current, power = self.run_motor(
                pattern=pattern,
                voltage=float(row['voltage']),
                current=float(row['current']),
                runtime=float(row['runtime']),
                reverse=reverse
            )

            results.append({
                'motor_id': motor_id,
                'config_num': k+1,
                'pattern': str(pattern),
                'reverse': reverse,
                'voltage': float(row['voltage']),
                'current': float(row['current']),
                'runtime': float(row['runtime']),
                'measured_voltage': voltage,
                'measured_current': current,
                'measured_power': power,
                'success': return_code == 0
            })

            col_idx = motor_col_map.get(motor_id, 0)
            with result_cols[col_idx]:
                if return_code != 0:
                    st.warning(f'{motor_id} failed with code {return_code}')
                else:
                    if voltage is not None and current is not None and power is not None:
                        st.success(f'{motor_id}: V={voltage:.2f}V, I={current:.2f}A, P={power:.2f}W')
                    else:
                        st.success(f'{motor_id} completed successfully')

            if k < len(motor_configs) - 1:
                time.sleep(2)

        return pd.DataFrame(results)


# ----- UTILITY FUNCTIONS

def lookup_vin(vin):
    v = Client()
    vehicle = v.decode_vin(vin)
    y = str(vehicle['ModelYear'])
    mk = str(vehicle['Make']).upper()
    mod = str(vehicle['Model']).upper()
    t = str(vehicle['Trim']).upper() if vehicle['Trim'] != 'Unknown' else ''
    return ' '.join([y, mk, mod, t])


# ----- CONFIG LOADER

@st.cache_data
def load_motor_configs(config_path='motor_configs.json'):
    """Load motor configurations from a JSON file and return as DataFrames."""
    if not os.path.exists(config_path):
        st.error(f'Motor config file not found: {config_path}')
        st.stop()

    with open(config_path, 'r') as f:
        raw = json.load(f)

    configs = {}

    # Load all simple configs (lists of row dicts) as DataFrames
    simple_keys = [
        'motor_0_up', 'motor_0_bump', 'motor_0_down',
        'motor_1_up', 'motor_1_bump', 'motor_1_down',
        'motor_2_up', 'motor_2_bump', 'motor_2_down',
        'motor_3_up', 'motor_3_bump', 'motor_3_down',
        'all_motors_down', 'stop_all_motors'
    ]
    for key in simple_keys:
        configs[key] = pd.DataFrame(raw[key])

    # Build multi_motor_sequence from its step references
    seq_steps = raw['multi_motor_sequence']['steps']
    frames = [
        configs[step['config']].iloc[[step['row']]]
        for step in seq_steps
    ]
    configs['multi_motor_sequence'] = pd.concat(frames, axis=0, ignore_index=True)

    return configs


# ----- LOAD CONFIGS

cfg = load_motor_configs('motor_configs.json')

motor_0_up            = cfg['motor_0_up']
motor_0_bump          = cfg['motor_0_bump']
motor_0_down          = cfg['motor_0_down']
motor_1_up            = cfg['motor_1_up']
motor_1_bump          = cfg['motor_1_bump']
motor_1_down          = cfg['motor_1_down']
motor_2_up            = cfg['motor_2_up']
motor_2_bump          = cfg['motor_2_bump']
motor_2_down          = cfg['motor_2_down']
motor_3_up            = cfg['motor_3_up']
motor_3_bump          = cfg['motor_3_bump']
motor_3_down          = cfg['motor_3_down']
all_motors_down       = cfg['all_motors_down']
stop_all_motors       = cfg['stop_all_motors']
multi_motor_sequence  = cfg['multi_motor_sequence']


# ----- STREAMLIT UI

st.markdown(""" <style> [data-testid="stDecoration"] { display: none; } </style>""", unsafe_allow_html=True)
st.set_page_config(page_title='Wheeljack', page_icon=':wrench:', initial_sidebar_state='collapsed', menu_items=None)
try:
    st.logo('Logo.png', size='large')
except:
    pass

if 'motor_configs' not in st.session_state:
    st.session_state.motor_configs = None
if 'results' not in st.session_state:
    st.session_state.results = None
if 'stop_requested' not in st.session_state:
    st.session_state.stop_requested = False

tab1, tab2, tab3, tab4 = st.tabs(['INTAKE', 'LIFT', 'SECURE', 'METADATA'])

with tab1:
    with st.form(key='form_0'):
        col11, col12, col13, col14 = st.columns(4)
        with col11:
            st.session_state.car = st.text_input('Car number')
        with col12:
            st.session_state.vin = st.text_input('VIN')
        with col13:
            st.session_state.tire_code = st.text_input('Tire code')
        with col14:
            st.session_state.rim_width = st.number_input('Rim width')
            submitted_0 = st.form_submit_button(label='START', type='primary', width='stretch')
        if submitted_0:
            st.info(lookup_vin(st.session_state.vin))

with tab2:
    st.info('Lift controls will be implemented here')

with tab3:
    st.warning('Ensure the vehicle is secure on all 4 jackstands by raising Motor 0, Motor 1, Motor 2, and Motor 3 in order OR by running the Multi-motor sequence')

    test_mode_options = {
        1: 'Normal mode (both devices required)',
        2: 'Test power supply only (no Rigol required)',
        3: 'Test motor control only (no relay board required)',
        4: 'Full test (no hardware required)'
    }

    selected_mode = st.selectbox(
        'Select Operating Mode:',
        options=list(test_mode_options.keys()),
        format_func=lambda x: test_mode_options[x],
        index=3
    )

    MODE = Mode(selected_mode)

    col1, col2, col3, col4 = st.columns(4)

    with col1:
        st.write(':blue[Motor 0]')
        if st.button('BUMP', icon=':material/expand_circle_up:', key='motor_0_bump', use_container_width=True):
            st.session_state.motor_configs = motor_0_bump
            st.session_state.run_sequence = True
        if st.button('UP', icon=':material/arrow_circle_up:', key='motor_0_up', use_container_width=True):
            st.session_state.motor_configs = motor_0_up
            st.session_state.run_sequence = True
        if st.button('DOWN', icon=':material/arrow_circle_down:', key='motor_0_down', use_container_width=True):
            st.session_state.motor_configs = motor_0_down
            st.session_state.run_sequence = True

    with col2:
        st.write(':blue[Motor 1]')
        if st.button('BUMP', icon=':material/expand_circle_up:', key='motor_1_bump', use_container_width=True):
            st.session_state.motor_configs = motor_1_bump
            st.session_state.run_sequence = True
        if st.button('UP', icon=':material/arrow_circle_up:', key='motor_1_up', use_container_width=True):
            st.session_state.motor_configs = motor_1_up
            st.session_state.run_sequence = True
        if st.button('DOWN', icon=':material/arrow_circle_down:', key='motor_1_down', use_container_width=True):
            st.session_state.motor_configs = motor_1_down
            st.session_state.run_sequence = True

    with col3:
        st.write(':blue[Motor 2]')
        if st.button('BUMP', icon=':material/expand_circle_up:', key='motor_2_bump', use_container_width=True):
            st.session_state.motor_configs = motor_2_bump
            st.session_state.run_sequence = True
        if st.button('UP', icon=':material/arrow_circle_up:', key='motor_2_up', use_container_width=True):
            st.session_state.motor_configs = motor_2_up
            st.session_state.run_sequence = True
        if st.button('DOWN', icon=':material/arrow_circle_down:', key='motor_2_down', use_container_width=True):
            st.session_state.motor_configs = motor_2_down
            st.session_state.run_sequence = True

    with col4:
        st.write(':blue[Motor 3]')
        if st.button('BUMP', icon=':material/expand_circle_up:', key='motor_3_bump', use_container_width=True):
            st.session_state.motor_configs = motor_3_bump
            st.session_state.run_sequence = True
        if st.button('UP', icon=':material/arrow_circle_up:', key='motor_3_up', use_container_width=True):
            st.session_state.motor_configs = motor_3_up
            st.session_state.run_sequence = True
        if st.button('DOWN', icon=':material/arrow_circle_down:', key='motor_3_down', use_container_width=True):
            st.session_state.motor_configs = motor_3_down
            st.session_state.run_sequence = True

    st.write("---")

    col_empty1, col5, col6, col7 = st.columns(4)

    with col5:
        if st.button('MULTI-MOTOR', icon=':material/cycle:', key='multi_motor', use_container_width=True):
            st.session_state.motor_configs = multi_motor_sequence
            st.session_state.run_sequence = True

    with col6:
        if st.button('ALL DOWN', icon=':material/arrow_circle_down:', key='all_down', use_container_width=True):
            st.session_state.motor_configs = all_motors_down
            st.session_state.run_sequence = True

    with col7:
        if st.button('STOP ALL', icon=':material/dangerous:', key='stop_all', use_container_width=True):
            st.session_state.stop_requested = True
            st.session_state.motor_configs = stop_all_motors
            st.session_state.run_sequence = True

    if 'run_sequence' in st.session_state and st.session_state.run_sequence:
        st.session_state.run_sequence = False
        is_stop_command = st.session_state.stop_requested

        if st.session_state.motor_configs is None:
            st.error('No valid sequence selected')
        else:
            try:
                status_col1, status_col2, status_col3, status_col4 = st.columns(4)

                if is_stop_command:
                    with status_col4:
                        st.error('STOPPING ALL MOTORS', icon=':material/dangerous:')
                else:
                    with status_col1:
                        st.badge('Initializing', color='blue')

                controller = MotorController(executable='wheeljack.exe', test_mode=MODE)

                if not is_stop_command:
                    with status_col2:
                        st.badge(controller.init_messages[0], color='blue')
                    with status_col3:
                        st.write(MODE)
                    with status_col4:
                        st.badge('Running', color='green')

                results = controller.run_sequence(st.session_state.motor_configs)
                st.session_state.results = results
                st.session_state.stop_requested = False

            except FileNotFoundError as e:
                st.error(f'Error: {e}')
                st.info('Please ensure "wheeljack.exe" is in the current directory.')
            except Exception as e:
                st.error(f'Error during execution: {e}')

with tab4:
    try:
        metadata = wheeljack_metadata.play()
        st.dataframe(metadata, use_container_width=True)
    except Exception as e:
        st.error(e)