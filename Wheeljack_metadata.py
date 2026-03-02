#<Filename>: <Wheeljack_metadata.py>
#<Author>:   <DANIEL DESAI>
#<Updated>:  <2026-02-12>
#<Version>:  <0.0.3>
#
# Usage:
#     import Wheeljack_metadata as wheeljack_metadata
#     wheeljack_metadata.play()
#

import streamlit as st
import pandas as pd
from datetime import datetime
import re


def convert_fraction_to_decimal(text):
    # Convert fractional entries to decimal form
    # Examples: 
    #     17 3/8 -> 17.375
    #     3/8 -> 0.375
    #     17.5 -> 17.5 (unchanged)
    
    # Handle non-string inputs (like floats or numbers)
    if not isinstance(text, str):
        return str(text) if text else ''
    text = text.strip()
    # Return empty string if input is empty
    if not text:
        return ''
    # Pattern for whole number + fraction: '17 3/8'
    pattern1 = r'^(\d+)\s+(\d+)/(\d+)$'
    match = re.match(pattern1, text)
    if match:
        whole = int(match.group(1))
        numerator = int(match.group(2))
        denominator = int(match.group(3))
        if denominator != 0:
            decimal = whole + (numerator / denominator)
            return str(decimal)
    # Pattern for just fraction: '3/8'
    pattern2 = r'^(\d+)/(\d+)$'
    match = re.match(pattern2, text)
    if match:
        numerator = int(match.group(1))
        denominator = int(match.group(2))
        if denominator != 0:
            decimal = numerator / denominator
            return str(decimal)
    # Return unchanged if not a fraction
    return text


def initialize_session_state():
    # Initialize session state variables
    if 'metadata_records' not in st.session_state:
        st.session_state.metadata_records = []
    # Safety check: ensure it's a list
    if not isinstance(st.session_state.metadata_records, list):
        st.session_state.metadata_records = []
    if 'metadata_pending_copy' not in st.session_state:
        st.session_state.metadata_pending_copy = None
    if 'metadata_pending_clear' not in st.session_state:
        st.session_state.metadata_pending_clear = None
    if 'metadata_trigger_download' not in st.session_state:
        st.session_state.metadata_trigger_download = None
    # Initialize form fields if not already set
    defaults = {
        'car': '',
        'vin': '',
        'tire_code': '',
        'rim_width': '',
        'df_m2_diameter': '',
        'df_m1_diameter': '',
        'df_m2_y': '',
        'df_m1_minus_m2_y': '',
        'df_camber': '',
        'pf_m2_diameter': '',
        'pf_m1_diameter': '',
        'pf_m2_y': '',
        'pf_m1_minus_m2_y': '',
        'pf_camber': '',
        'dr_m2_diameter': '',
        'dr_m1_diameter': '',
        'dr_m2_y': '',
        'dr_m1_minus_m2_y': '',
        'dr_camber': '',
        'pr_m2_diameter': '',
        'pr_m1_diameter': '',
        'pr_m2_y': '',
        'pr_m1_minus_m2_y': '',
        'pr_camber': '',
    }
    
    # Apply pending clear if it exists (process this FIRST)
    if st.session_state.metadata_pending_clear:
        clear_info = st.session_state.metadata_pending_clear

        if not clear_info['keep_header']:
            st.session_state.metadata_car = ''
            st.session_state.metadata_vin = ''
            st.session_state.metadata_tech = ''
        
        st.session_state.metadata_date = clear_info['date']
        st.session_state.metadata_tire_code = ''
        st.session_state.metadata_rim_width = ''
        
        # Clear all wheel measurements
        for wheel in ['df', 'pf', 'dr', 'pr']:
            for field in ['m2_diameter', 'm1_diameter', 'm2_y', 'm1_minus_m2_y', 'camber']:
                st.session_state[f'metadata_{wheel}_{field}'] = ''
        
        st.session_state.metadata_pending_clear = None
    
    # Apply pending copy if it exists (process AFTER clear)
    if st.session_state.metadata_pending_copy:
        for field, value in st.session_state.metadata_pending_copy.items():
            for wheel in ['pf', 'dr', 'pr']:
                st.session_state[f'metadata_{wheel}_{field}'] = value
        st.session_state.metadata_pending_copy = None
    
    for key, default_value in defaults.items():
        if f'metadata_{key}' not in st.session_state:
            st.session_state[f'metadata_{key}'] = default_value


def copy_to_all_wheels():
    # Copy driver front wheel data to all other wheels
    fields = ['m2_diameter', 'm1_diameter', 'm2_y', 'm1_minus_m2_y', 'camber']

    # Check if driver front has data
    has_data = any(st.session_state.get(f'metadata_df_{field}', '') for field in fields)
    #
    if not has_data:
        st.warning('Please enter data in Driver Front Wheel first!')
        return
    # Store values to copy in a pending state
    st.session_state.metadata_pending_copy = {
        field: st.session_state.get(f'metadata_df_{field}', '')
        for field in fields
    }
    #
    st.success('Driver Front wheel data copied to all wheels!')


def save_and_export_record():
    # Save current form data and prepare CSV for export
    # Convert fractions in all numeric fields before saving
    
    # Ensure metadata_records is a list
    if not isinstance(st.session_state.metadata_records, list):
        st.session_state.metadata_records = []
    
    data = {
        'Timestamp': datetime.now().replace(microsecond=0).isoformat(),
        'Car': st.session_state.get('metadata_car', ''),
        'VIN': st.session_state.get('metadata_vin', ''),
        'Tire_code': st.session_state.get('metadata_tire_code', ''),
        'Rim_width': convert_fraction_to_decimal(st.session_state.get('metadata_rim_width', '')),
        'DF_M2_diameter': convert_fraction_to_decimal(st.session_state.get('metadata_df_m2_diameter', '')),
        'DF_M1_diameter': convert_fraction_to_decimal(st.session_state.get('metadata_df_m1_diameter', '')),
        'DF_M2_y': convert_fraction_to_decimal(st.session_state.get('metadata_df_m2_y', '')),
        'DF_M1_minus_M2_y': convert_fraction_to_decimal(st.session_state.get('metadata_df_m1_minus_m2_y', '')),
        'DF_camber': convert_fraction_to_decimal(st.session_state.get('metadata_df_camber', '')),
        'PF_M2_diameter': convert_fraction_to_decimal(st.session_state.get('metadata_pf_m2_diameter', '')),
        'PF_M1_diameter': convert_fraction_to_decimal(st.session_state.get('metadata_pf_m1_diameter', '')),
        'PF_M2_y': convert_fraction_to_decimal(st.session_state.get('metadata_pf_m2_y', '')),
        'PF_M1_minus_M2_y': convert_fraction_to_decimal(st.session_state.get('metadata_pf_m1_minus_m2_y', '')),
        'PF_camber': convert_fraction_to_decimal(st.session_state.get('metadata_pf_camber', '')),
        'DR_M2_diameter': convert_fraction_to_decimal(st.session_state.get('metadata_dr_m2_diameter', '')),
        'DR_M1_diameter': convert_fraction_to_decimal(st.session_state.get('metadata_dr_m1_diameter', '')),
        'DR_M2_y': convert_fraction_to_decimal(st.session_state.get('metadata_dr_m2_y', '')),
        'DR_M1_minus_M2_y': convert_fraction_to_decimal(st.session_state.get('metadata_dr_m1_minus_m2_y', '')),
        'DR_camber': convert_fraction_to_decimal(st.session_state.get('metadata_dr_camber', '')),
        'PR_M2_diameter': convert_fraction_to_decimal(st.session_state.get('metadata_pr_m2_diameter', '')),
        'PR_M1_diameter': convert_fraction_to_decimal(st.session_state.get('metadata_pr_m1_diameter', '')),
        'PR_M2_y': convert_fraction_to_decimal(st.session_state.get('metadata_pr_m2_y', '')),
        'PR_M1_minus_m2_y': convert_fraction_to_decimal(st.session_state.get('metadata_pr_m1_minus_m2_y', '')),
        'PR_camber': convert_fraction_to_decimal(st.session_state.get('metadata_pr_camber', '')),
    }    
    st.session_state.metadata_records.append(data)
    # Clear form
    clear_form(keep_header=True)


def clear_form(keep_header=False):
    # Clear all form fields
    today = datetime.now().strftime('%m/%d/%Y')
    
    # Store what should be cleared in pending state
    st.session_state.metadata_pending_clear = {
        'keep_header': keep_header,
        'date': today
    }


def export_to_csv():
    # Export records to CSV
    if not st.session_state.metadata_records:
        return None
    
    df = pd.DataFrame(st.session_state.metadata_records)
    csv = df.to_csv(index=False)
    return csv


def play():
    # Main function
    initialize_session_state()
    
    # Pre-fill from parent session state on first load only
    if 'car' in st.session_state:
        st.session_state.metadata_car = st.session_state.car
    if 'vin' in st.session_state:
        st.session_state.metadata_vin = st.session_state.vin
    if 'tire_code' in st.session_state:
        st.session_state.metadata_tire_code = st.session_state.tire_code
    if 'rim_width' in st.session_state:
        st.session_state.metadata_rim_width = st.session_state.rim_width
    
    # Mark as initialized
    st.session_state.metadata_initialized = True
    
    # Wheel sections
    col21, col22, col23, col24 = st.columns(4)
    
    with col21:
        st.write('**:blue[DRIVER FRONT]**')
        df_m2_diameter = st.text_input('M2 DIAMETER', key='metadata_df_m2_diameter',
                                       help='You can enter fractions like \'17 3/8\'')
        df_m1_diameter = st.text_input('M1 DIAMETER', key='metadata_df_m1_diameter')
        df_m2_y = st.text_input('M2 (Y)', key='metadata_df_m2_y')
        df_m1_minus_m2_y = st.text_input('M1 - M2 (Y)', key='metadata_df_m1_minus_m2_y')
        df_camber = st.text_input('CAMBER', key='metadata_df_camber')
    
    with col22:
        st.write('**:blue[PASSENGER FRONT]**')
        pf_m2_diameter = st.text_input('M2 DIAMETER ', key='metadata_pf_m2_diameter')
        pf_m1_diameter = st.text_input('M1 DIAMETER ', key='metadata_pf_m1_diameter')
        pf_m2_y = st.text_input('M2 (Y) ', key='metadata_pf_m2_y')
        pf_m1_minus_m2_y = st.text_input('M1 - M2 (Y) ', key='metadata_pf_m1_minus_m2_y')
        pf_camber = st.text_input('CAMBER ', key='metadata_pf_camber')
    
    with col23:
        st.write('**:blue[DRIVER REAR]**')
        dr_m2_diameter = st.text_input('M2 DIAMETER  ', key='metadata_dr_m2_diameter')
        dr_m1_diameter = st.text_input('M1 DIAMETER  ', key='metadata_dr_m1_diameter')
        dr_m2_y = st.text_input('M2 (Y)  ', key='metadata_dr_m2_y')
        dr_m1_minus_m2_y = st.text_input('M1 - M2 (Y)  ', key='metadata_dr_m1_minus_m2_y')
        dr_camber = st.text_input('CAMBER  ', key='metadata_dr_camber')
    
    with col24:
        st.write('**:blue[PASSENGER REAR]**')
        pr_m2_diameter = st.text_input('M2 DIAMETER   ', key='metadata_pr_m2_diameter')
        pr_m1_diameter = st.text_input('M1 DIAMETER   ', key='metadata_pr_m1_diameter')
        pr_m2_y = st.text_input('M2 (Y)   ', key='metadata_pr_m2_y')
        pr_m1_minus_m2_y = st.text_input('M1 - M2 (Y)   ', key='metadata_pr_m1_minus_m2_y')
        pr_camber = st.text_input('CAMBER   ', key='metadata_pr_camber')
    
    st.divider()
    
    # Action buttons - all in one row
    col1, col2, col3, col4 = st.columns(4)
    
    with col1:
        if st.button('Copy Driver Front to All Wheels', type='secondary', use_container_width=True):
            copy_to_all_wheels()
            st.rerun()
    
    with col2:
        if st.button('Save Record', type='secondary', use_container_width=True):
            save_and_export_record()
            #st.rerun()
    
    with col3:
        # Generate CSV from existing records for download button
        if st.session_state.metadata_records:
            csv_data = pd.DataFrame(st.session_state.metadata_records).to_csv(index=False)
            
            st.download_button(
                label='Export CSV',
                data=csv_data,
                file_name=f'vehicle_metadata_{datetime.now().strftime("%Y%m%d_%H%M%S")}.csv',
                mime='text/csv',
                type='primary',
                use_container_width=True
            )
        else:
            st.button('Export CSV', type='primary', use_container_width=True, disabled=True)
    
    with col4:
        if st.button('Clear Form', type='secondary', use_container_width=True):
            clear_form()
            st.rerun()

    # Return saved records
    if st.session_state.metadata_records:
        df = pd.DataFrame(st.session_state.metadata_records)
        # st.dataframe(df, use_container_width=True)  # --- UNCOMMENT TO RUN IN STANDALONE MODE
        return df
    else:
        return None
    
    
# Standalone mode for testing
if __name__ == '__main__':
    # Example: Set session state values as if from another tab
    if 'car' not in st.session_state:
        st.session_state.car = '001'
    if 'vin' not in st.session_state:
        st.session_state.vin = '1HGBH41JXMN109186'
    if 'tire_code' not in st.session_state:
        st.session_state.tire_code = '225/45R17'
    if 'rim_width' not in st.session_state:
        st.session_state.rim_width = '17.5'
    
    play()