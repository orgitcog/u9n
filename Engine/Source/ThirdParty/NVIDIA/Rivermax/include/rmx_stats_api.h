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

#ifndef RMX_STATS_API_H_
#define RMX_STATS_API_H_

#include "rivermax_api.h"
#include "rmx_stats_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup RivermaxStats */
/** @{ */
//=============================================================================

/**
 * @brief Initialize Rivermax statistics functionality
 * @return Status code as defined by @ref rmx_status
 * @remark Call this function only by external applications that do not use
 *         Rivermax API and do not call to @ref rmx_init.
 * @warning If @ref rmx_init is not used, It SHALL be called before any other
 *          Rivermax stats API.
 */
__export
rmx_status rmx_stats_init(void);

/**
 * @brief Performs Rivermax statistict utility cleanups
 *
 * This routine releases the resources allocated by the Rivermax statistics
 * utility.
 *
 * @return Status code as defined by @ref rmx_status
 * @remark Call this function only by external applications that
 *         do not use Rivermax API. Applications that use @ref rmx_init should
 *         use @ref rmx_cleanup instead of this function.
 * @warning An application cannot call any Rivermax statistics routine after
 *          @ref rmx_stats_cleanup call.
 */
__export
rmx_status rmx_stats_cleanup(void);

/** @defgroup StatMessageHandle Statistics message handle */
/** @{ */
//-----------------------------------------------------------------------------

/**
 * @brief Initialize statistics message handle
 * @param[in,out] message   Statistics message handle
 */
__export
void rmx_stats_init_message(rmx_stats_message *message);

/**
 * @brief Get statistics message type
 * @param[in] message   Statistics message handle
 * @return Statistics message type defined by @ref rmx_stats_type
 */
__export
rmx_stats_type rmx_stats_get_type(const rmx_stats_message *message);

/**
 * @brief Get statistics message session ID
 * @param[in] message   Statistics message handle
 * @return uint16_t Statistics message session ID
 * @remark upon error RMX_STATS_INVALID_SESSION_ID is returned
 */
__export
uint16_t rmx_stats_get_session_id(const rmx_stats_message *message);

/**
 * @brief Get statistics message thread ID
 * @param[in] message   Statistics message handle
 * @return uint32_t Statistics message thread ID
 * @remark upon error RMX_STATS_INVALID_THREAD_ID is returned
 */
__export
uint32_t rmx_stats_get_thread_id(const rmx_stats_message *message);

/**
 * @brief Get statistics message process ID
 * @param[in] message   Statistics message handle
 * @return uint32_t Statistics message process ID
 * @remark upon error RMX_STATS_INVALID_PROCESS_ID is returned
 */
__export
uint32_t rmx_stats_get_process_id(const rmx_stats_message *message);

/**
 * @brief Get statistics message time
 * @param[in] message   Statistics message handle
 * @return uint64_t Statistics message time
 * @note Time field is currently not in use.
 */
__export
uint64_t rmx_stats_get_time(const rmx_stats_message *message);

//-----------------------------------------------------------------------------
/**@} StatMessageHandle */

/** @defgroup StatDataHandle Statistics data handle */
/** @{ */
//-----------------------------------------------------------------------------

/**
 * @brief Get session start statistics data handle
 * @param[in] message   Statistics message handle
 * @return @ref rmx_stats_session_start_handle Statistics data handle
 */
__export
const rmx_stats_session_start_handle *rmx_stats_get_session_start_handle(const rmx_stats_message *message);

/**
 * @brief Get session stop statistics data handle
 * @param[in] message   Statistics message handle
 * @return @ref rmx_stats_session_stop_handle Statistics data handle
 */
__export
const rmx_stats_session_stop_handle *rmx_stats_get_session_stop_handle(const rmx_stats_message *message);

/**
 * @brief Get session runtime statistics data handle
 * @param[in] message   Statistics message handle
 * @return @ref rmx_stats_session_runtime_handle Statistics data handle
 */
__export
const rmx_stats_session_runtime_handle *rmx_stats_get_session_runtime_handle(const rmx_stats_message *message);

/**
 * @brief Get TX gueue statistics data handle
 * @param[in] message   Statistics message handle
 * @return @ref rmx_stats_tx_queue_handle Statistics data handle
 */
__export
const rmx_stats_tx_queue_handle *rmx_stats_get_tx_queue_handle(const rmx_stats_message *message);

/**
 * @brief Get RX queue statistics data handle
 * @param[in] message   Statistics message handle
 * @return @ref rmx_stats_rx_queue_handle Statistics data handle
 */
__export
const rmx_stats_rx_queue_handle *rmx_stats_get_rx_queue_handle(const rmx_stats_message *message);
//-----------------------------------------------------------------------------
/**@} StatDataHandle */

/** @defgroup StatSessionStart Session start statistics */
/** @{ */
//-----------------------------------------------------------------------------

/**
 * @brief Get session source addresses from session start statistics message
 * @param[in] handle   Start session statistics data handle
 * @return Pointer to the begining of source sockaddr_storage array
 */
__export
const struct sockaddr_storage *rmx_stats_get_start_src_addresses(const rmx_stats_session_start_handle *handle);

/**
 * @brief Get session destination addresses from session start statistics message
 * @param[in] handle   Start session statistics data handle
 * @return Pointer to the begining of destination sockaddr_storage array
 */
__export
const struct sockaddr_storage *rmx_stats_get_start_dst_addresses(const rmx_stats_session_start_handle *handle);

/**
 * @brief Get number of media streams from session start statistics message
 * @param[in] handle   Start session statistics data handle
 * @return uint16_t Number of media streams
 */
__export
uint16_t rmx_stats_get_start_media_streams(const rmx_stats_session_start_handle *handle);

/**
 * @brief Get media type from session start statistics message
 * @param[in] handle   Start session statistics data handle
 * @return Media type as defined by @ref rmx_media_type
 */
__export
rmx_media_type rmx_stats_get_start_media_type(const rmx_stats_session_start_handle *handle);

/**
 * @brief Get media clock rate from session start statistics message
 * @param[in] handle   Start session statistics data handle
 * @return uint16_t Media clock rate
 */
__export
uint32_t rmx_stats_get_start_media_clock_rate(const rmx_stats_session_start_handle *handle);

/**
 * @brief Get video height from session start statistics message
 * @param[in] handle   Start session statistics data handle
 * @return uint16_t Video height
 */
__export
uint16_t rmx_stats_get_start_video_height(const rmx_stats_session_start_handle *handle);

/**
 * @brief Get video width from session start statistics message
 * @param[in] handle   Start session statistics data handle
 * @return uint16_t Video width
 */
__export
uint16_t rmx_stats_get_start_video_width(const rmx_stats_session_start_handle *handle);

/**
 * @brief Get video scan type from session start statistics message
 * @param[in] handle   Start session statistics data handle
 * @return Video scan type as defined by @ref rmx_stats_scan_type
 */
__export
rmx_stats_scan_type rmx_stats_get_start_video_scan_type(const rmx_stats_session_start_handle *handle);

/**
 * @brief Get video cmax from session start statistics message
 * @param[in] handle   Start session statistics data handle
 * @return uint16_t video cmax
 */
__export
uint16_t rmx_stats_get_start_video_cmax(const rmx_stats_session_start_handle *handle);

/**
 * @brief Get number of video frames per second from session start statistics
 *        message
 * @param[in] handle   Start session statistics data handle
 * @return float Number of video frames per second
 */
__export
float rmx_stats_get_start_video_frames_per_second(const rmx_stats_session_start_handle *handle);

/**
 * @brief Get audio ptime from session start statistics message
 * @param[in] handle   Start session statistics data handle
 * @return float Audio ptime
 */
__export
float rmx_stats_get_start_audio_ptime(const rmx_stats_session_start_handle *handle);

/**
 * @brief Get audio bit depth from session start statistics message
 * @param[in] handle   Start session statistics data handle
 * @return uint16_t Audio bit depth
 */
__export
uint16_t rmx_stats_get_start_audio_bit_depth(const rmx_stats_session_start_handle *handle);

/**
 * @brief Get audio channels from session start statistics message
 * @param[in] handle   Start session statistics data handle
 * @return uint16_t Number of audio channels
 */
__export
uint16_t rmx_stats_get_start_audio_channels(const rmx_stats_session_start_handle *handle);

//-----------------------------------------------------------------------------
/**@} StatSessionStart */

/** @defgroup StatSessionStop Session stop statistics */
/** @{ */
//-----------------------------------------------------------------------------

/**
 * @brief Get total number of transfered packets from session stop statistics message
 * @param[in] handle   Stop session statistics data handle
 * @return size_t Total number of transfered packets
 */
__export
size_t rmx_stats_get_stop_transfered_packets(const rmx_stats_session_stop_handle *handle);

/**
 * @brief Get total number of transfered bytes from session stop statistics
 *        message
 * @param[in] handle   Stop session statistics data handle
 * @return size_t Total number of transfered bytes
 */
__export
size_t rmx_stats_get_stop_transfered_bytes(const rmx_stats_session_stop_handle *handle);

/**
 * @brief Get allocated memory from session stop statistics message
 * @param[in] handle   Stop session statistics data handle
 * @return size_t Allocated memory
 */
__export
size_t rmx_stats_get_stop_allocated_memory(const rmx_stats_session_stop_handle *handle);

/**
 * @brief Get Number of memory blocks from session stop statistics message
 * @param[in] handle   Stop session statistics data handle
 * @return size_t Number of memory blocks
 */
__export
size_t rmx_stats_get_stop_memory_blocks(const rmx_stats_session_stop_handle *handle);

/**
 * @brief Get session status from session stop statistics message
 * @param[in] handle   Stop session statistics data handle
 * @return rmx_status Session status
 */
__export
rmx_status rmx_stats_get_stop_status(const rmx_stats_session_stop_handle *handle);


//-----------------------------------------------------------------------------
/**@} StatSessionStart */

/** @defgroup StatSessionRuntime Session runtime statistics */
/** @{ */
//-----------------------------------------------------------------------------

/**
 * @brief Get total number of the session commited chunks
 *        from session runtime statistics message
 * @param[in] handle   Session runtime statistics data handle
 * @return size_t Total number of the session commited chunks
 */
__export
size_t rmx_stats_get_runtime_committed_chunks(const rmx_stats_session_runtime_handle *handle);

/**
 * @brief Get total number of the session commited strides from session runtime
 *        statistics message
 * @param[in] handle   Session runtime statistics data handle
 * @return size_t Total number of the session commited strides
 */
__export
size_t rmx_stats_get_runtime_committed_strides(const rmx_stats_session_runtime_handle *handle);

/**
 * @brief Get total number of the session requests notifications from session
 *        runtime statistics message
 * @param[in] handle   Session runtime statistics data handle
 * @return size_t total number of the session requests notifications
 */
__export
size_t rmx_stats_get_runtime_requests_notifications(const rmx_stats_session_runtime_handle *handle);

/**
 * @brief Get session type from session runtime statistics message
 * @param[in] handle   Session runtime statistics data handle
 * @return Session type as defined by @ref rmx_stats_session_type
 */
__export
rmx_stats_session_type rmx_stats_get_runtime_session_type(const rmx_stats_session_runtime_handle *handle);

/**
 * @brief Get total number of the session user chunks from session runtime
 *        statistics message
 * @param[in] handle   Session runtime statistics data handle
 * @return uint32_t Total number of the session user chunks
 */
__export
uint32_t rmx_stats_get_runtime_user_chunks(const rmx_stats_session_runtime_handle *handle);

/**
 * @brief Get total number of the session free chunks from session runtime
 *        statistics message
 * @param[in] handle   Session runtime statistics data handle
 * @return uint32_t Total number of the session free chunks
 */
__export
uint32_t rmx_stats_get_runtime_free_chunks(const rmx_stats_session_runtime_handle *handle);

/**
 * @brief Get total number of the session busy chunks from session runtime
 *        statistics message
 * @param[in] handle   Session runtime statistics data handle
 * @return uint32_t Total number of the session busy chunks
 */
__export
uint32_t rmx_stats_get_runtime_busy_chunks(const rmx_stats_session_runtime_handle *handle);

//-----------------------------------------------------------------------------
/**@} StatSessionRuntime */


/** @defgroup StatSessionTxQueue TX queue statistics */
/** @{ */
//-----------------------------------------------------------------------------

/**
 * @brief Get number of TX packets from TX queue statistics message
 * @param[in] handle   TX queue statistics data handle
 * @return size_t Number of TX packets
 */
__export
size_t rmx_stats_get_tx_queue_num_packets(const rmx_stats_tx_queue_handle *handle);

/**
 * @brief Get number of TX bytes from TX queue statistics message
 * @param[in] handle   TX queue statistics data handle
 * @return size_t Number of TX bytes
 */
__export
size_t rmx_stats_get_tx_queue_num_bytes(const rmx_stats_tx_queue_handle *handle);

/**
 * @brief Get number of packet wqes from TX queue statistics message
 * @param[in] handle   TX queue statistics data handle
 * @return size_t Number of packet wqes
 */
__export
size_t rmx_stats_get_tx_queue_packet_wqes(const rmx_stats_tx_queue_handle *handle);

/**
 * @brief Get number of dummy wqes from TX queue statistics message
 * @param[in] handle   TX queue statistics data handle
 * @return size_t Number of dummy wqes
 */
__export
size_t rmx_stats_get_tx_queue_dummy_wqes(const rmx_stats_tx_queue_handle *handle);

/**
 * @brief Get number of free wqes from TX queue statistics message
 * @param[in] handle   TX queue statistics data handle
 * @return size_t Number of free wqes
 */
__export
uint32_t rmx_stats_get_tx_queue_free_wqes(const rmx_stats_tx_queue_handle *handle);

/**
 * @brief Get number of credits used for delay correction from TX queue statistics message
 *
 * The delay calculated as the difference between the time the chunks
 * were meant to be sent and the time they were actually sent.
 *
 * @param[in] handle   TX queue statistics data handle
 * @return size_t Number of credits used for delay correction
 */
__export
uint64_t rmx_stats_get_tx_queue_delay_correction_credits(const rmx_stats_tx_queue_handle *handle);

/**
 * @brief Get number of transmission delays from TX queue statistics message
 *
 * The delay calculated as the difference between the time the chunks
 * were meant to be sent and the time they were actually sent.
 *
 * @param[in] handle   TX queue statistics data handle
 * @return size_t Number of transmission delays
 */
__export
uint64_t rmx_stats_get_tx_queue_num_transmission_delays(const rmx_stats_tx_queue_handle *handle);

/**
* @brief Get minimum transmission delay in ns from TX queue statistics message
*
* The delay calculated as the difference between the time the chunks
* were meant to be sent and the time they were actually sent.
*
* @param[in] handle   TX queue statistics data handle
* @return size_t Min delay in nanoseconds
*/
__export
uint64_t rmx_stats_get_tx_queue_min_transmission_delay_ns(const rmx_stats_tx_queue_handle *handle);

/**
 * @brief Get maximum transmission delay in ns from TX queue statistics message
 *
 * The delay calculated as the difference between the time the chunks
 * were meant to be sent and the time they were actually sent.
 *
 * @param[in] handle   TX queue statistics data handle
 * @return size_t Max delay in nanoseconds
 */
__export
uint64_t rmx_stats_get_tx_queue_max_transmission_delay_ns(const rmx_stats_tx_queue_handle *handle);

/**
 * @brief Get average transmission delay in ns from TX queue statistics message
 *
 * The delay calculated as the difference between the time the chunks
 * were meant to be sent and the time they were actually sent.
 *
 * @param[in] handle   TX queue statistics data handle
 * @return size_t Avg delay in nanoseconds
 */
__export
uint64_t rmx_stats_get_tx_queue_avg_transmission_delay_ns(const rmx_stats_tx_queue_handle *handle);

//-----------------------------------------------------------------------------
/**@} StatSessionTxQueue */

/** @defgroup StatSessionRxQueue RX queue statistics */
/** @{ */
//-----------------------------------------------------------------------------

/**
 * @brief Get number of RX packets from RX queue statistics message
 * @param[in] handle   RX queue statistics data handle
 * @return size_t Number of RX packets
 */
__export
size_t rmx_stats_get_rx_queue_num_packets(const rmx_stats_rx_queue_handle * handle);

/**
 * @brief Get number of RX bytes from RX queue statistics message
 * @param[in] handle   RX queue statistics data handle
 * @return size_t Number of RX bytes
 */
__export
size_t rmx_stats_get_rx_queue_num_bytes(const rmx_stats_rx_queue_handle * handle);

/**
 * @brief Get number of used strides from RX queue statistics message
 * @param[in] handle   RX queue statistics data handle
 * @return size_t Number of used strides
 */
__export
uint32_t rmx_stats_get_rx_queue_used_strides(const rmx_stats_rx_queue_handle * handle);

/**
 * @brief Get number of wqe strides from RX queue statistics message
 * @param[in] handle   RX queue statistics data handle
 * @return size_t Number of wqe strides
 */
__export
uint32_t rmx_stats_get_rx_queue_wqe_strides(const rmx_stats_rx_queue_handle * handle);

/**
 * @brief Get number of RX crc errors from RX queue statistics message
 * @param[in] handle   RX queue statistics data handle
 * @return size_t Number of RX crc errors
 */
__export
size_t rmx_stats_get_rx_queue_crc_errors(const rmx_stats_rx_queue_handle * handle);

//-----------------------------------------------------------------------------
/**@} StatSessionRxQueue */

/** @defgroup StatConsumerConfig Statistics consumer configuration */
/** @{ */
//-----------------------------------------------------------------------------

/**
 * @brief Initialize statistics config handle
 *
 * The Config handle is used to configure the statistics consumer.
 *
 * @param[in,out] config   Statistics configuration handle
 * @remark Currently all configuration should be done before using the
 *         config handle to initialize the statistics consumer using
 *         @ref rmx_stats_init_consumer
 */
__export
void rmx_stats_init_config(rmx_stats_config *config);

/**
 * @brief Clear registered statistics message types
 * @param[in] config   Statistics configuration handle
 */
__export
void rmx_stats_config_clear_registered_stats_types(rmx_stats_config *config);


/**
 * @brief Register to receive statistics message type
 * @param[in] config   Statistics configuration handle
 * @return Status code as defined by @ref rmx_status
 */
__export
rmx_status rmx_stats_config_register_stats_type(rmx_stats_config *config, rmx_stats_type type);

/**
 * @brief Set Rivermax statistics process id
 * @param[in] config   Statistics configuration handle
 * @note Zero process id (default) means local process id
 */
__export
void rmx_stats_config_set_process_id(rmx_stats_config *config, uint32_t process_id);

//-----------------------------------------------------------------------------
/**@} StatConsumerConfig */

/** @defgroup StatConsumer Statistics consumer */
/** @{ */
//-----------------------------------------------------------------------------

/**
 * @brief Initialize statistics consumer handle
 *
 * Statistics consumer is used to read statistics messages
 *
 * @param[in,out] consumer   Statistics consumer handle
 * @param[in]     config     Statistics configuration handle
 */
__export
void rmx_stats_init_consumer(rmx_stats_consumer *consumer, rmx_stats_config *config);

/**
 * @brief Create statistics consumer handle
 * @param[in,out] consumer   Statistics consumer handle
 * @param[in]     config     Statistics configuration handle
 * @return Status code as defined by @ref rmx_status
 * @note On creation, the consumer is attached to statistics
 *       messages shared memory according to the process id
 *       encapsulated by the config parameter
 */
__export
rmx_status rmx_stats_create_consumer(rmx_stats_consumer *consumer, rmx_stats_config *config);

/**
 * @brief Pop one statistics message and copy it to the given
 *        message handler
 * @param[in]     consumer   Statistics consumer handle
 * @param[in,out] message    Statistics message handle
 * @return Status code as defined by @ref rmx_status
 */
__export
rmx_status rmx_stats_consumer_pop_message(rmx_stats_consumer *consumer, rmx_stats_message *message);

/**
 * @brief Destroy statistics consumer
 * @param[in]  consumer   Statistics consumer handle
 */
__export
void rmx_stats_destroy_consumer(rmx_stats_consumer *consumer);


//-----------------------------------------------------------------------------
/**@} StatConsumer */

//
//=============================================================================
/** @} RivermaxStats */



#ifdef __cplusplus
}
#endif

#endif /* RMX_STATS_API_H_ */
