#ifndef TCI_EVENTS_h
#define TCI_EVENTS_h

#include "global.h"
#include "tci.h"

void on_connect_disconnect_event() {
    if(tci.connected()) {
        digitalWrite(TCI_CONNECTED_PIN, HIGH);        
        Serial.println("== TCI: Connected");
    } else {
        digitalWrite(TCI_CONNECTED_PIN, LOW);
        Serial.println("== TCI: Disconnected");
    }  
}

// ===== Please, for better parameters understanding
// ===== read the TCI official documentation on
// ===== https://github.com/ExpertSDR3/TCI
//
// ===== TCI commands implementation follows the same order 
// ===== described in the above document

//
// ********************** INITIALIZATIONS COMMANDS ***********************
//

// ===== VFO_LIMITS =====
void on_device_vfo_limits_event(int low_limit, int high_limit) {
    Serial.printf("== TCI: vfo_limits event - low_limit:%d - high_limit:%d\n", low_limit, high_limit);	
}
//tci.get_vfo_low_limit()   -- returns vfo low limit
//tci.get_vfo_high_limit()  -- returns vfo high limit

// ===== IF_LIMITS =====
void on_device_if_limits_event(int low_limit, int high_limit) {
    Serial.printf("== TCI: if_limits event - low_limit:%d - high_limit:%d\n", low_limit, high_limit);	
}
//tci.get_if_low_limit()   -- returns if low limit
//tci.get_if_high_limit()  -- returns if high limit

// ===== TRX_COUNT =====
void on_trx_count_event(int trx_count) {
    Serial.printf("== TCI: trx_count event - trx_count:%d\n", trx_count);	    
}

// ===== CHANNELS_COUNT =====
void on_channels_count_event(int channels_count) {
    Serial.printf("== TCI: channels_count event - channels_count:%d\n", channels_count);	    
}

// ===== DEVICE =====
void on_device_event() {
    //Be sure to define a buffer variable with at least 20 characters
    char local_device_name[20];
    int name_len = 0;
    tci.get_device(local_device_name, &name_len, sizeof(local_device_name));
    Serial.printf("== TCI: device event - name:%s - len:%d\n", 
                   local_device_name,name_len);	    
}

// ===== RECEIVE_ONLY =====
void on_receive_only_event() {
    Serial.printf("== TCI: receive_only event - %s\n",
                  tci.is_receive_only() ? "true":"false"); 
}

// ===== MODULATIONS_LIST =====
void on_modulations_list_event() {
    //Be sure to define a buffer variable with at least 90 characters
    char local_modulations_list_buffer[90];
    int buf_len = 0;
    tci.get_modulations_list(local_modulations_list_buffer, &buf_len, sizeof(local_modulations_list_buffer));
    Serial.printf("== TCI: modulations list event - values:%s - len:%d\n", 
                   local_modulations_list_buffer,buf_len);	 
    char *modulation_value;    
    const char sep[2] = ",";             
    modulation_value = strtok(local_modulations_list_buffer, sep);

    Serial.printf("        Modulation values: ");
    while( modulation_value != NULL ) {
      Serial.printf( "%s - ", modulation_value );    
      modulation_value = strtok(NULL, sep);
    }
   Serial.printf("<\n");
}

// ===== PROTOCOL =====
void on_protocol_event() {
    //Be sure to define a buffer variable with at least 20 characters
    char local_protocol_name[20];
    int name_len = 0;
    tci.get_protocol(local_protocol_name, &name_len, sizeof(local_protocol_name));
    Serial.printf("== TCI: protocol event - Name: %s - len: %d\n", 
                   local_protocol_name,name_len);    
}

// ===== READY =====
void on_ready_event() {
    Serial.printf("== TCI: device ready: %s\n",
                   tci.is_ready() ? "true" : "false");
}

//
// ********************** BIDIRECTIONAL CONTROL COMMANDS ***********************
//

// ===== START =====
void on_start_event() {   
    Serial.printf("== TCI: start event - %d - started\n", tci.is_started());
}

// ===== STOP =====
void on_stop_event() {   
    Serial.printf("== TCI: stop event - %d - stopped\n", tci.is_started());
}

// ===== DDS =====
void on_dds_event(int rtxId) {
    Serial.printf("== TCI: DDS event - rtxId:%d - DDS freq:%d\n", rtxId, tci.rtx[rtxId].getDds());
} 

// ===== IF =====
void on_if_event(int rtxId, int vfoId) {
    Serial.printf("== TCI: IF event - rtxId:%d - vfoId:%d - IF freq:%d\n", rtxId, vfoId, tci.rtx[rtxId].getIf(vfoId));
}

// ===== VFO =====
void on_vfo_event(int rtxId, int vfoId) {
    Serial.printf("== TCI: VFO event - rtxId:%d - vfoId:%d - VFO freq:%d\n", rtxId, vfoId, tci.rtx[rtxId].getVfo(vfoId));
}

// ===== MODULATION =====
void on_modulation_event(int rtxId) {
    Serial.printf("== TCI: modulation event - rtxId:%d - modulation:%s\n", rtxId, tci.rtx[rtxId].getModulation());
}

// ===== TRX =====
void on_trx_event(int rtxId) {
    Serial.printf("== TCI: trx event - rtxId:%d - value:%s\n",
                    rtxId,
                    tci.is_ready() ? "true" : "false");
}

// ===== TUNE =====
void on_tune_event(int rtxId) {
    Serial.printf("== TCI: tune event - rtxId:%d - value:%s\n",
                rtxId,
                tci.rtx[rtxId].getTune() ? "true" : "false");
}

// ===== DRIVE =====
void on_drive_event(int rtxId) {
    Serial.printf("== TCI: drive event - rtxId:%d - power:%d%%\n", rtxId, tci.rtx[rtxId].getDrive());
}

// ===== TUNE_DRIVE =====
void on_tune_drive_event(int rtxId) {
    Serial.printf("== TCI: tune drive event - rtxId:%d - power:%d%%\n", rtxId, tci.rtx[rtxId].getTuneDrive());
}


// ===== RIT_ENABLE =====
void on_rit_enable_event(int rtxId) {
    Serial.printf("== TCI: rit_enable event - rtxId:%d - value:%s\n",
                rtxId,
                tci.rtx[rtxId].getRitEnable() ? "true" : "false");
}

// ===== XIT_ENABLE =====
void on_xit_enable_event(int rtxId) {
    Serial.printf("== TCI: xit_enable event - rtxId:%d - value:%s\n",
                rtxId,
                tci.rtx[rtxId].getXitEnable() ? "true" : "false");
}

// ===== SPLIT_ENABLE =====
void on_split_enable_event(int rtxId) {
    Serial.printf("== TCI: split_enable event - rtxId:%d - value:%s\n",
                rtxId,
                tci.rtx[rtxId].getSplitEnable() ? "true" : "false");
}

// ===== RIT_OFFSET =====
void on_rit_offset_event(int rtxId) {
    Serial.printf("== TCI: rit_offset event - rtxId:%d - freq:%d\n", rtxId, tci.rtx[rtxId].getRitOffset());
}

// ===== XIT_OFFSET =====
void on_xit_offset_event(int rtxId) {
    Serial.printf("== TCI: xit_offset event - rtxId:%d - freq:%d\n", rtxId, tci.rtx[rtxId].getXitOffset());
}

// ===== RX_CHANNEL_ENABLE =====
void on_rx_channel_enable_event(int rtxId, int vfoId) {
    Serial.printf("== TCI: rx_channel_enable event - rtxId:%d - vfoId:%d - enabled:%d\n", rtxId, vfoId, tci.rtx[rtxId].getRxChannelEnable(vfoId));
}

// ===== RX_FILTER_BAND =====
void on_rx_filter_band_event(int rtxId) {
    Serial.printf("== TCI: rx_filter_band event - rtxId:%d - filter [%d - %d]\n", 
                  rtxId, 
                  tci.rtx[rtxId].getRxFilterLower(),
                  tci.rtx[rtxId].getRxFilterTop());
}

// ===== CW_MACROS_SPEED =====
void on_cw_macros_speed_event() {
    Serial.printf("== TCI: cw_macros_speed event - speed:%d\n", tci.get_cw_macros_speed());
}

// ===== CW_MACROS_DELAY =====
void on_cw_macros_delay_event() {
    Serial.printf("== TCI: cw_macros_delay event - milliseconds:%d\n", tci.get_cw_macros_delay());
}

// ===== CW_KEYER_SPEED =====
void on_cw_keyer_speed_event() {
    Serial.printf("== TCI: cw_keyer_speed event - speed:%d\n", tci.get_cw_keyer_speed());
}

// ===== VOLUME =====
void on_volume_event() {
    Serial.printf("== TCI: volume event - dB value:%d\n", tci.get_volume());
}

// ===== MUTE =====
void on_mute_event() {
    Serial.printf("== TCI: mute event - value:%d\n", tci.is_mute());
}

// ===== RX_MUTE =====
void on_rx_mute_event(int rtxId) {
    Serial.printf("== TCI: rx_mute event - rtxId:%d - value:%d\n", rtxId, tci.rtx[rtxId].getRxMute());
}

// ===== RX_VOLUME =====
void on_rx_volume_event(int rtxId, int vfoId) {
    Serial.printf("== TCI: rx_volume event - rtxId:%d - vfoId:%d - dB value:%d\n", rtxId, vfoId, tci.rtx[rtxId].getRxVolume(vfoId));
}

// ===== RX_BALANCE =====
void on_rx_balance_event(int rtxId, int vfoId) {
    Serial.printf("== TCI: rx_balance event - rtxId:%d - vfoId:%d - dB value:%d\n", rtxId, vfoId, tci.rtx[rtxId].getRxBalance(vfoId));
}

// ===== MON_VOLUME =====
void on_mon_volume_event() {
    Serial.printf("== TCI: mon_volume event - dB value:%d\n", tci.get_mon_volume());
}

// ===== MON_ENABLE =====
void on_mon_enable_event() {
    Serial.printf("== TCI: mon_enable event - value:%d\n", tci.is_mon_enable());
}

// ===== AGC_MODE =====
void on_agc_mode_event(int rtxId) {
    switch (tci.rtx[rtxId].getAgcMode()) {
        case 0:
            Serial.printf("== TCI: agc_mode event - rtxId:%d - value:off\n", rtxId);
            break;
        case 1:
            Serial.printf("== TCI: agc_mode event - rtxId:%d - value:fast\n", rtxId);
            break;
        case 2:
            Serial.printf("== TCI: agc_mode event - rtxId:%d - value:normal\n", rtxId);
            break;                         
    }
}

// ===== AGC_GAIN =====
void on_agc_gain_event(int rtxId) {
    Serial.printf("== TCI: agc_gain event - rtxId:%d - dB value:%d\n", rtxId, tci.rtx[rtxId].getAgcGain());
}

// ===== RX_NB_ENABLE =====
void on_rx_nb_enable_event(int rtxId) {
    Serial.printf("== TCI: rx_nb_enable event - rtxId:%d - value:%d\n", rtxId, tci.rtx[rtxId].isRxNbEnable());
}

// ===== RX_NB_PARAM =====
void on_rx_nb_param_event(int rtxId) {
    Serial.printf("== TCI: rx_volume event - rtxId:%d - triggering threshold:%d - pulse duratioin:%d\n",
                   rtxId, tci.rtx[rtxId].getTriggeringThresold(), tci.rtx[rtxId].getPulseDuration());
}

// ===== RX_BIN_ENABLE =====
void on_rx_bin_enable_event(int rtxId) {
    Serial.printf("== TCI: rx_bin_enable event - rtxId:%d - value:%s\n", 
                  rtxId, 
                  tci.rtx[rtxId].isRxBinEnable() ? "true" : "false");
}

// ===== RX_NR_ENABLE =====
void on_rx_nr_enable_event(int rtxId) {
    Serial.printf("== TCI: rx_nr_enable event - rtxId:%d - value:%s\n", 
                  rtxId, 
                  tci.rtx[rtxId].isRxNrEnable() ? "true" : "false");
}

// ===== RX_ANC_ENABLE =====
void on_rx_anc_enable_event(int rtxId) {
    Serial.printf("== TCI: rx_anc_enable event - rtxId:%d - value:%s\n", 
                  rtxId, 
                  tci.rtx[rtxId].isRxAncEnable() ? "true" : "false");
}

// ===== RX_ANF_ENABLE =====
void on_rx_anf_enable_event(int rtxId) {
    Serial.printf("== TCI: rx_anf_enable event - rtxId:%d - value:%s\n", 
                  rtxId, 
                  tci.rtx[rtxId].isRxAnfEnable() ? "true" : "false");
}

// ===== RX_APF_ENABLE =====
void on_rx_apf_enable_event(int rtxId) {
    Serial.printf("== TCI: rx_apf_enable event - rtxId:%d - value:%s\n", 
                  rtxId, 
                  tci.rtx[rtxId].isRxApfEnable() ? "true" : "false");
}

// ===== RX_DSE_ENABLE =====
void on_rx_dse_enable_event(int rtxId) {
    Serial.printf("== TCI: rx_dse_enable event - rtxId:%d - value:%s\n", 
                  rtxId, 
                  tci.rtx[rtxId].isRxDseEnable() ? "true" : "false");
}

// ===== RX_NF_ENABLE =====
void on_rx_nf_enable_event(int rtxId) {
    Serial.printf("== TCI: rx_nf_enable event - rtxId:%d - value:%s\n", 
                  rtxId, 
                  tci.rtx[rtxId].isRxNfEnable() ? "true" : "false");
}

// ===== LOCK =====
void on_lock_event(int rtxId) {
    Serial.printf("== TCI: lock event - rtxId:%d - value:%s\n", 
                  rtxId, 
                  tci.rtx[rtxId].isLock() ? "true" : "false");
}

// ===== SQL_ENABLE =====
void on_sql_enable_event(int rtxId) {
    Serial.printf("== TCI: sql_enable event - rtxId:%d - value:%s\n", 
                  rtxId, 
                  tci.rtx[rtxId].getSqlEnable() ? "true" : "false");
}

// ===== AGC_GAIN =====
void on_sql_level_event(int rtxId) {
    Serial.printf("== TCI: sql_level event - rtxId:%d - dB value:%s\n", 
                  rtxId, 
                  tci.rtx[rtxId].getSqlLevel() ? "true" : "false");
}

//
// ********************** UNIDIRECTIONAL CONTROL COMMANDS ***********************
//

// ===== TX_ENABLE =====
void on_tx_enable_event(int rtxId) {
    Serial.printf("== TCI: tx_enabled event - rtxId:%d - enabled:%s\n", 
                  rtxId, 
                  tci.rtx[rtxId].getTxEnable() ? "true" : "false");
}

// ===== IQ_SAMPLERATE =====
void on_iq_samplerate_event() {
    Serial.printf("== TCI: iq_samplerate event - value:%d\n", tci.get_iq_samplerate());
}

// ===== AUDIO_SAMPLERATE =====
void on_audio_samplerate_event() {
    Serial.printf("== TCI: audio_samplerate event - value:%d\n", tci.get_audio_samplerate());
}

// ===== IQ_START/IQ_STOP =====
void on_iq_start_stop_event_event(int rtxId) {
    Serial.printf("== TCI: iq_start_stop_event - rtxId:%d - %s\n",
                  rtxId,
                  tci.rtx[rtxId].isIqStarted() ? "true":"false");
}

// ===== AUDIO_START/AUDIO_STOP =====
void on_audio_start_stop_event_event(int rtxId) {
    Serial.printf("== TCI: audio_start_stop_event - rtxId:%d - %s\n",
                  rtxId,
                  tci.rtx[rtxId].isAudioStarted() ? "true":"false");
}

// ===== LINE_OUT_START/LINE_OUT_STOP =====
void on_line_out_start_stop_event(int rtxId) {
    Serial.printf("== TCI: line_out_start_stop_event - rtxId:%d - %s\n",
                  rtxId,
                  tci.rtx[rtxId].isLineOutStarted() ? "true":"false");
}

// ===== LINE_OUT_RECORDER_START =====
void on_line_out_recorder_start_event(int rtxId) {
    Serial.printf("== TCI: line_out_recorder_start_event - rtxId:%d - %d\n",
                  rtxId,
                  tci.rtx[rtxId].getRecordDuration());
}

// ===== LINE_OUT_RECORDER_SAVE =====
void on_line_out_recorder_save_event(int rtxId) {
    Serial.printf("== TCI: line_out_recorder_stop_event - rtxId:%d - %s\n",
                  rtxId,
                  tci.rtx[rtxId].getRecordFileName());
}

// ===== LINE_OUT_RECORDER_BREAK =====
void on_line_out_recorder_break_event(int rtxId) {
    Serial.printf("== TCI: line_out_recorder_break_event - rtxId:%d\n",rtxId);
}


//
// ********************** NOTIFICATIONS COMMANDS ***********************
//

// ===== CLICKED_ON_SPOT =====
void on_clicked_on_spot_event() {
    spot new_spot = tci.get_clicked_spot();
    Serial.printf("== TCI: clicked_on_spot event - callsign:%s - frequency:%u\n", 
                  new_spot.callsing,
                  new_spot.frequency);
}

void on_rx_clicked_on_spot_event() {
    spot new_spot = tci.get_clicked_spot();
    Serial.printf("== TCI: rx_clicked_on_spot event - rtxId:%d - vfoId: %d - callsign:%s - frequency:%u\n", 
                  new_spot.rtxId,
                  new_spot.vfoId,
                  new_spot.callsing,
                  new_spot.frequency);
}

// ===== TX_FOOTSWITCH =====
void on_tx_footswitch_event(int rtxId) {
    if (tci.rtx[rtxId].isTxFootswitch())
        Serial.printf("== TCI: tx_footswitch event - rtxId:%d - enabled\n", rtxId);
    else
        Serial.printf("== TCI: tx_footswitch event - rtxId:%d - not enabled\n", rtxId);
}

// ===== TX_FREQUENCY =====
void on_tx_frequency_event() {
        Serial.printf("== TCI: tx_frequency event - frequency:%d - enabled\n", 
                      tci.get_tx_frequency());
}

// ===== APP_FOCUS =====
void on_app_focus_event() {
    Serial.printf("== TCI: app_focus event - value:%s\n", 
                  tci.is_app_focus() ? "true" : "false");
}

// ===== RX_SENSORS =====
void on_rx_sensors_event(int rtxId, int dbm_level) {

    Serial.printf("== TCI: rx_sensors event - rtxId:%d - signal level: %.1f dBm\n", 
                    rtxId,
                    ((float)dbm_level)/10);
}
    
// ===== TX_SENSORS =====
void on_tx_sensors_event(int rtxId, float mic_level, 
                         float rms_power_out, float peak_power_out, float swr) {

    Serial.printf("== TCI: tx_sensors event - rtxId:%d - mic level:%.1f dBm - rms_power_out:%.1fW - peak_power_out:%.1fW - SWR:%.1f\n", 
                    rtxId, mic_level, rms_power_out, peak_power_out, swr);
}

// =========================================================
// =========================================================

void configure_tci_events()
{
  tci.attach_conn_disc_event(on_connect_disconnect_event);  
  tci.attach_vfo_event(on_vfo_event);
}

#endif
