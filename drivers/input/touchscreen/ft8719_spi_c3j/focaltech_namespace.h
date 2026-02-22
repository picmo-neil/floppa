#ifndef _FOCALTECH_FTS_C3J_NAMESPACE_H_
#define _FOCALTECH_FTS_C3J_NAMESPACE_H_

/*
 * Namespace C3J FocalTech touchscreen driver symbols so it can be linked
 * built-in alongside other FocalTech driver trees under LTO.
 */

/* global data */
#define fts_data                        c3j_fts_data

/* firmware blobs / upgrade tables */
#define fw_file                         c3j_fw_file
#define fw_file2                        c3j_fw_file2
#define fw_file3                        c3j_fw_file3
#define module_list                     c3j_module_list
#define upgrade_func_list               c3j_upgrade_func_list
#define fwupgrade                       c3j_fwupgrade

/* ini/test globals */
#define ic_types                        c3j_ic_types

/* bus / reg access */
#define fts_read                        c3j_fts_read
#define fts_write                       c3j_fts_write
#define fts_read_reg                    c3j_fts_read_reg
#define fts_write_reg                   c3j_fts_write_reg
#define fts_bus_init                    c3j_fts_bus_init
#define fts_bus_exit                    c3j_fts_bus_exit

/* core helpers */
#define fts_wait_tp_to_valid            c3j_fts_wait_tp_to_valid
#define fts_tp_state_recovery           c3j_fts_tp_state_recovery
#define fts_reset_proc                  c3j_fts_reset_proc
#define fts_irq_disable                 c3j_fts_irq_disable
#define fts_irq_enable                  c3j_fts_irq_enable
#define fts_hid2std                     c3j_fts_hid2std
#define fts_release_all_finger          c3j_fts_release_all_finger
#define fts_input_event                 c3j_fts_input_event

/* ex_fun/sysfs */
#define fts_create_apk_debug_channel    c3j_fts_create_apk_debug_channel
#define fts_release_apk_debug_channel   c3j_fts_release_apk_debug_channel
#define fts_create_sysfs                c3j_fts_create_sysfs
#define fts_remove_sysfs                c3j_fts_remove_sysfs

/* ex_mode */
#define fts_ex_mode_recovery            c3j_fts_ex_mode_recovery
#define fts_ex_mode_init                c3j_fts_ex_mode_init
#define fts_ex_mode_exit                c3j_fts_ex_mode_exit

/* gesture/fod */
#define fts_gesture_enable              c3j_fts_gesture_enable
#define fts_create_gesture_sysfs        c3j_fts_create_gesture_sysfs
#define fts_gesture_readdata            c3j_fts_gesture_readdata
#define fts_gesture_recovery            c3j_fts_gesture_recovery
#define fts_gesture_suspend             c3j_fts_gesture_suspend
#define fts_gesture_resume              c3j_fts_gesture_resume
#define fts_gesture_init                c3j_fts_gesture_init
#define fts_gesture_exit                c3j_fts_gesture_exit
#define fts_gesture_switch              c3j_fts_gesture_switch
#define fts_ts_is_gesture_mode          c3j_fts_ts_is_gesture_mode
#define fts_fod_readdata                c3j_fts_fod_readdata
#define fts_fod_recovery                c3j_fts_fod_recovery
#define fts_fod_suspend                 c3j_fts_fod_suspend
#define fts_fod_resume                  c3j_fts_fod_resume
#define fts_fod_pay                     c3j_fts_fod_pay

/* esdcheck */
#define fts_esdcheck_set_intr           c3j_fts_esdcheck_set_intr
#define fts_esdcheck_get_status         c3j_fts_esdcheck_get_status
#define fts_esdcheck_proc_busy          c3j_fts_esdcheck_proc_busy
#define fts_esdcheck_switch             c3j_fts_esdcheck_switch
#define fts_esdcheck_suspend            c3j_fts_esdcheck_suspend
#define fts_esdcheck_resume             c3j_fts_esdcheck_resume
#define fts_create_esd_sysfs            c3j_fts_create_esd_sysfs
#define fts_esdcheck_init               c3j_fts_esdcheck_init
#define fts_esdcheck_exit               c3j_fts_esdcheck_exit

/* point report check */
#define fts_prc_queue_work              c3j_fts_prc_queue_work
#define fts_point_report_check_init     c3j_fts_point_report_check_init
#define fts_point_report_check_exit     c3j_fts_point_report_check_exit

/* fw upgrade */
#define fts_fwupg_reset_in_boot         c3j_fts_fwupg_reset_in_boot
#define fts_fwupg_enter_into_boot       c3j_fts_fwupg_enter_into_boot
#define fts_fwupg_erase                 c3j_fts_fwupg_erase
#define fts_fwupg_ecc_cal               c3j_fts_fwupg_ecc_cal
#define fts_fwupg_upgrade               c3j_fts_fwupg_upgrade
#define fts_fwupg_init                  c3j_fts_fwupg_init
#define fts_fwupg_exit                  c3j_fts_fwupg_exit
#define fts_read_file                   c3j_fts_read_file
#define fts_upgrade_bin                 c3j_fts_upgrade_bin
#define fts_enter_test_environment      c3j_fts_enter_test_environment

/* ini/test exported helpers */
#define get_keyword_value               c3j_get_keyword_value
#define fts_test_get_testparam_from_ini c3j_fts_test_get_testparam_from_ini

/* focaltech_test core globals/helpers */
#define fts_ftest                       c3j_fts_ftest
#define test_func_list                  c3j_test_func_list
#define sys_delay                       c3j_sys_delay
#define focal_abs                       c3j_focal_abs
#define fts_malloc                      c3j_fts_malloc
#define fts_free_proc                   c3j_fts_free_proc
#define print_buffer                    c3j_print_buffer
#define fts_test_read_reg               c3j_fts_test_read_reg
#define fts_test_write_reg              c3j_fts_test_write_reg

/* focaltech_test additional helpers */
#define fts_test_read                   c3j_fts_test_read
#define fts_test_write                  c3j_fts_test_write
#define enter_work_mode                 c3j_enter_work_mode
#define enter_factory_mode              c3j_enter_factory_mode
#define read_mass_data                  c3j_read_mass_data
#define short_get_adcdata_incell        c3j_short_get_adcdata_incell
#define wait_state_update               c3j_wait_state_update
#define start_scan                      c3j_start_scan
#define get_rawdata                     c3j_get_rawdata
#define chip_clb                        c3j_chip_clb
#define get_cb_incell                   c3j_get_cb_incell
#define get_cb_sc                       c3j_get_cb_sc
#define compare_data                    c3j_compare_data
#define compare_array                   c3j_compare_array
#define show_data                       c3j_show_data
#define mapping_switch                  c3j_mapping_switch
#define get_fw_wp                       c3j_get_fw_wp
#define get_cb_mc_sc                    c3j_get_cb_mc_sc
#define get_rawdata_mc_sc               c3j_get_rawdata_mc_sc
#define get_rawdata_mc                  c3j_get_rawdata_mc
#define short_get_adc_data_mc           c3j_short_get_adc_data_mc
#define compare_mc_sc                   c3j_compare_mc_sc
#define show_data_mc_sc                 c3j_show_data_mc_sc
#define fts_test_entry                  c3j_fts_test_entry
#define fts_test_save_data              c3j_fts_test_save_data
#define fts_test_init                   c3j_fts_test_init
#define fts_test_exit                   c3j_fts_test_exit

#endif /* _FOCALTECH_FTS_C3J_NAMESPACE_H_ */
