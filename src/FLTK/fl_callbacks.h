// FLTK callback functions for butt
//
// Copyright 2007-2018 by Daniel Noethen.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#ifndef FL_CALLBACKS_H
#define FL_CALLBACKS_H

enum { STREAM_TIME = 0, REC_TIME, SENT_DATA, REC_DATA };
enum { STREAM = 0, RECORD };

class flgui;

extern int display_info;
extern flgui *fl_g;

void button_cfg_cb(void);
void button_info_cb(void);
void button_record_cb(void);
void button_connect_cb(void);
void button_disconnect_cb(void);
void button_add_icy_add_cb(void);
void button_cfg_del_srv_cb(void);
void button_cfg_del_icy_cb(void);
void choice_cfg_act_srv_cb(void);
void choice_cfg_act_icy_cb(void);
void button_cfg_add_srv_cb(void);
void button_cfg_add_icy_cb(void);
void choice_cfg_bitrate_cb(void);
void choice_cfg_samplerate_cb(void);
void button_cfg_song_go_cb(void);
void choice_cfg_codec_mp3_cb(void);
void choice_cfg_codec_ogg_cb(void);
void choice_cfg_codec_opus_cb(void);
void choice_cfg_codec_aac_cb(void);
void choice_cfg_codec_flac_cb(void);
void button_cfg_export_cb(void);
void button_cfg_import_cb(void);
void check_cfg_mono_to_stereo_cb(void);
void button_add_icy_save_cb(void);
void button_add_srv_cancel_cb(void);
void button_add_icy_cancel_cb(void);
void choice_cfg_channel_stereo_cb(void);
void choice_cfg_channel_mono_cb(void);
void button_cfg_browse_songfile_cb(void);
void input_cfg_song_file_cb(void);
void input_cfg_song_cb(void);
void input_cfg_song_prefix_cb(void);
void input_cfg_song_suffix_cb(void);
void input_cfg_buffer_cb(bool print_message);
void input_cfg_signal_cb(void);
void input_cfg_silence_cb(void);
void input_cfg_(void);
void input_cfg_present_level_cb(void);
void input_cfg_absent_level_cb(void);
void choice_cfg_resample_mode_cb(void);
void button_cfg_check_for_updates_cb(void);
void choice_cfg_dev_cb(void);
void button_cfg_rescan_devices_cb(void);
void radio_cfg_ID_cb(void);
void radio_cfg_name_cb(void);
void choice_cfg_right_channel_cb(void);
void choice_cfg_left_channel_cb(void);

void button_add_srv_add_cb(void);
void button_add_srv_save_cb(void);
void button_add_srv_show_pwd_cb(void);
void radio_add_srv_shoutcast_cb(void);
void radio_add_srv_icecast_cb(void);
void button_add_srv_revoke_cert_cb(void);
void button_start_agent_cb(void);
void button_stop_agent_cb(void);
void check_update_at_startup_cb(void);
void check_start_agent_cb(void);
void check_minimize_to_tray_cb(void);

void button_rec_browse_cb(void);
void button_rec_split_now_cb(void);
void choice_rec_bitrate_cb(void);
void choice_rec_samplerate_cb(void);
void choice_rec_channel_stereo_cb(void);
void choice_rec_channel_mono_cb(void);
void choice_rec_codec_mp3_cb(void);
void choice_rec_codec_ogg_cb(void);
void choice_rec_codec_wav_cb(void);
void choice_rec_codec_opus_cb(void);
void choice_rec_codec_aac_cb(void);
void choice_rec_codec_flac_cb(void);
void input_rec_signal_cb(void);
void input_rec_silence_cb(void);

void input_tls_cert_file_cb(void);
void input_tls_cert_dir_cb(void);
void button_tls_browse_file_cb(void);
void button_tls_browse_dir_cb(void);

void button_cfg_edit_srv_cb(void);
void button_cfg_edit_icy_cb(void);
void check_song_update_active_cb(void);
void check_read_last_line_cb(void);
void check_sync_to_full_hour_cb(void);

void input_rec_filename_cb(void);
void input_rec_folder_cb(void);
void input_rec_split_time_cb(void);
void input_log_filename_cb(void);
void button_cfg_log_browse_cb(void);

void check_gui_attach_cb(void);
void check_gui_ontop_cb(void);
void check_gui_hide_log_window_cb(void);
void check_gui_remember_pos_cb(void);
void check_gui_lcd_auto_cb(void);
void check_gui_start_minimized_cb(void);
void button_gui_bg_color_cb(void);
void button_gui_text_color_cb(void);
void choice_gui_language_cb(void);
void radio_gui_vu_gradient_cb(void);
void radio_gui_vu_solid_cb(void);


void slider_gain_cb(void);

void check_activate_eq_cb(void);
void slider_equalizer1_cb(double);
void slider_equalizer2_cb(double);
void slider_equalizer3_cb(double);
void slider_equalizer4_cb(double);
void slider_equalizer5_cb(double);
void slider_equalizer6_cb(double);
void slider_equalizer7_cb(double);
void slider_equalizer8_cb(double);
void slider_equalizer9_cb(double);
void slider_equalizer10_cb(double);

void check_activate_drc_cb(void);
void check_aggressive_mode_cb(void);
void slider_threshold_cb(double);
void slider_ratio_cb(double);
void slider_attack_cb(double);
void slider_release_cb(double);
void slider_makeup_cb(double);

void check_cfg_auto_start_rec_cb(void);
void check_cfg_auto_stop_rec_cb(void);
void check_cfg_rec_after_launch_cb(void);

void check_cfg_rec_hourly_cb(void);
void check_cfg_connect_cb(void);
void check_cfg_force_reconnecting_cb(void);

void lcd_rotate(void*);
void ILM216_cb(void);
void window_main_close_cb(void);

void check_cfg_use_app_cb(void);
void choice_cfg_app_cb(void);
void radio_cfg_artist_title_cb(void);
void radio_cfg_title_artist_cb(void);

void choice_cfg_song_delay_cb(void);


void update_song(void* user_data);
bool stop_recording(bool ask);




#endif

