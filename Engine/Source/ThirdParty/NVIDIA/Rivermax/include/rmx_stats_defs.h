/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#ifndef RMX_STATS_DEFS_H_
#define RMX_STATS_DEFS_H_

#include "rivermax_defs.h"

/** @brief Invalid session ID value */
#define RMX_STATS_INVALID_SESSION_ID  (uint16_t)(-1)
/** @brief Invalid thread ID value */
#define RMX_STATS_INVALID_THREAD_ID   0
/** @brief Invalid process ID value */
#define RMX_STATS_INVALID_PROCESS_ID  0

/**
 * @brief A descriptor for a statistics message
 */
typedef struct rmx_stats_message_v1 {
    RMX_PLACEHOLDER_ALIGNED(720);
} rmx_stats_message;

/**
 * @brief A descriptor for statistics consumer configuration
 */
typedef struct rmx_stats_config_v1 {
    RMX_PLACEHOLDER_ALIGNED(320);
} rmx_stats_config;

/**
 * @brief A descriptor for statistics consumer
 */
typedef struct rmx_stats_consumer_v1 {
    RMX_PLACEHOLDER_ALIGNED(200);
} rmx_stats_consumer;

/**
 * @brief statistics message type
 * @memberof StatMessageHandle
 */

typedef enum {
    RMX_STATS_SESSION_START = 1,
    RMX_STATS_SESSION_STOP = 2,
    RMX_STATS_SESSION_RUN = 3,
    RMX_STATS_TX_QUEUE = 4,
    RMX_STATS_RX_QUEUE = 5,
    RMX_STATS_TIME = 6,
    RMX_STATS_LAST
} rmx_stats_type;

typedef enum {
    RMX_SESSION_TYPE_INVALID = 0,
    RMX_SESSION_TX = 1,
    RMX_SESSION_RX = 2,
} rmx_stats_session_type;

typedef enum
{
    RMX_MEDIA_TYPE_UNKNOWN,
    RMX_MEDIA_TYPE_VIDEO_2110_20,
    RMX_MEDIA_TYPE_VIDEO_2110_22,
    RMX_MEDIA_TYPE_VIDEO_2022_06,
    RMX_MEDIA_TYPE_VIDEO_2022_08,
    RMX_MEDIA_TYPE_AUDIO,
    RMX_MEDIA_TYPE_ANCILLARY,
} rmx_media_type;

typedef enum {
    RMX_SCAN_TYPE_UNSPECIFIED,
    RMX_SCAN_TYPE_INTERLACE,
    RMX_SCAN_TYPE_PSF,
    RMX_SCAN_TYPE_PROGRESSIVE,
} rmx_stats_scan_type;

/**
 * @typedef rmx_stats_session_start_handle
 * @brief A handle to session_start statistics data
 */
typedef struct rmx_stats_session_start rmx_stats_session_start_handle;

/**
 * @typedef rmx_stats_session_stop_handle
 * @brief A handle to session_stop statistics data
 */
typedef struct rmx_stats_session_stop rmx_stats_session_stop_handle;

/**
 * @typedef rmx_stats_session_runtime_handle
 * @brief A handle to session_runtime statistics data
 */
typedef struct rmx_stats_session_runtime rmx_stats_session_runtime_handle;

/**
 * @typedef rmx_stats_tx_queue_handle
 * @brief A handle to TX queue statistics data
 */
typedef struct rmx_stats_tx_queue rmx_stats_tx_queue_handle;

/**
 * @typedef rmx_stats_rx_queue_handle
 * @brief A handle to RX queue statistics data
 */
typedef struct rmx_stats_rx_queue rmx_stats_rx_queue_handle;

/** @cond VERSION_MAPPING */
#define rmx_stats_init                                    rmx_stats_init_v1
#define rmx_stats_cleanup                                 rmx_stats_cleanup_v1
#define rmx_stats_init_message                            rmx_stats_init_message_v1
#define rmx_stats_get_type                                rmx_stats_get_type_v1
#define rmx_stats_get_session_id                          rmx_stats_get_session_id_v1
#define rmx_stats_get_thread_id                           rmx_stats_get_thread_id_v1
#define rmx_stats_get_process_id                          rmx_stats_get_process_id_v1
#define rmx_stats_get_time                                rmx_stats_get_time_v1
#define rmx_stats_get_session_start_handle                rmx_stats_get_session_start_handle_v1
#define rmx_stats_get_session_stop_handle                 rmx_stats_get_session_stop_handle_v1
#define rmx_stats_get_session_runtime_handle              rmx_stats_get_session_runtime_handle_v1
#define rmx_stats_get_tx_queue_handle                     rmx_stats_get_tx_queue_handle_v1
#define rmx_stats_get_rx_queue_handle                     rmx_stats_get_rx_queue_handle_v1
#define rmx_stats_get_start_src_addresses                 rmx_stats_get_start_src_addresses_v1
#define rmx_stats_get_start_dst_addresses                 rmx_stats_get_start_dst_addresses_v1
#define rmx_stats_get_start_media_streams                 rmx_stats_get_start_media_streams_v1
#define rmx_stats_get_start_media_type                    rmx_stats_get_start_media_type_v1
#define rmx_stats_get_start_media_clock_rate              rmx_stats_get_start_media_clock_rate_v1
#define rmx_stats_get_start_video_height                  rmx_stats_get_start_video_height_v1
#define rmx_stats_get_start_video_width                   rmx_stats_get_start_video_width_v1
#define rmx_stats_get_start_video_scan_type               rmx_stats_get_start_video_scan_type_v1
#define rmx_stats_get_start_video_cmax                    rmx_stats_get_start_video_cmax_v1
#define rmx_stats_get_start_video_frames_per_second       rmx_stats_get_start_video_frames_per_second_v1
#define rmx_stats_get_start_audio_ptime                   rmx_stats_get_start_audio_ptime_v1
#define rmx_stats_get_start_audio_bit_depth               rmx_stats_get_start_audio_bit_depth_v1
#define rmx_stats_get_start_audio_channels                rmx_stats_get_start_audio_channels_v1
#define rmx_stats_get_stop_transfered_packets             rmx_stats_get_stop_transfered_packets_v1
#define rmx_stats_get_stop_transfered_bytes               rmx_stats_get_stop_transfered_bytes_v1
#define rmx_stats_get_stop_allocated_memory               rmx_stats_get_stop_allocated_memory_v1
#define rmx_stats_get_stop_memory_blocks                  rmx_stats_get_stop_memory_blocks_v1
#define rmx_stats_get_stop_status                         rmx_stats_get_stop_status_v1
#define rmx_stats_get_runtime_committed_chunks            rmx_stats_get_runtime_committed_chunks_v1
#define rmx_stats_get_runtime_committed_strides           rmx_stats_get_runtime_committed_strides_v1
#define rmx_stats_get_runtime_requests_notifications      rmx_stats_get_runtime_requests_notifications_v1
#define rmx_stats_get_runtime_session_type                rmx_stats_get_runtime_session_type_v1
#define rmx_stats_get_runtime_user_chunks                 rmx_stats_get_runtime_user_chunks_v1
#define rmx_stats_get_runtime_free_chunks                 rmx_stats_get_runtime_free_chunks_v1
#define rmx_stats_get_runtime_busy_chunks                 rmx_stats_get_runtime_busy_chunks_v1
#define rmx_stats_get_tx_queue_num_packets                rmx_stats_get_tx_queue_num_packets_v1
#define rmx_stats_get_tx_queue_num_bytes                  rmx_stats_get_tx_queue_num_bytes_v1
#define rmx_stats_get_tx_queue_packet_wqes                rmx_stats_get_tx_queue_packet_wqes_v1
#define rmx_stats_get_tx_queue_dummy_wqes                 rmx_stats_get_tx_queue_dummy_wqes_v1
#define rmx_stats_get_tx_queue_free_wqes                  rmx_stats_get_tx_queue_free_wqes_v1
#define rmx_stats_get_tx_queue_delay_correction_credits   rmx_stats_get_tx_queue_delay_correction_credits_v1
#define rmx_stats_get_tx_queue_num_transmission_delays    rmx_stats_get_tx_queue_num_transmission_delays_v1
#define rmx_stats_get_tx_queue_min_transmission_delay_ns  rmx_stats_get_tx_queue_min_transmission_delay_ns_v1
#define rmx_stats_get_tx_queue_max_transmission_delay_ns  rmx_stats_get_tx_queue_max_transmission_delay_ns_v1
#define rmx_stats_get_tx_queue_avg_transmission_delay_ns  rmx_stats_get_tx_queue_avg_transmission_delay_ns_v1
#define rmx_stats_get_rx_queue_num_packets                rmx_stats_get_rx_queue_num_packets_v1
#define rmx_stats_get_rx_queue_num_bytes                  rmx_stats_get_rx_queue_num_bytes_v1
#define rmx_stats_get_rx_queue_used_strides               rmx_stats_get_rx_queue_used_strides_v1
#define rmx_stats_get_rx_queue_wqe_strides                rmx_stats_get_rx_queue_wqe_strides_v1
#define rmx_stats_get_rx_queue_crc_errors                 rmx_stats_get_rx_queue_crc_errors_v1
#define rmx_stats_init_config                             rmx_stats_init_config_v1
#define rmx_stats_config_clear_registered_stats_types     rmx_stats_config_clear_registered_stats_types_v1
#define rmx_stats_config_register_stats_type              rmx_stats_config_register_stats_type_v1
#define rmx_stats_config_set_process_id                   rmx_stats_config_set_process_id_v1
#define rmx_stats_init_consumer                           rmx_stats_init_consumer_v1
#define rmx_stats_create_consumer                         rmx_stats_create_consumer_v1
#define rmx_stats_consumer_pop_message                    rmx_stats_consumer_pop_message_v1
#define rmx_stats_destroy_consumer                        rmx_stats_destroy_consumer_v1
/** @endcond */

#endif /* RMX_STATS_DEFS_H_ */

