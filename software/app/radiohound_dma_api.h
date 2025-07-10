/**************************************************************************************************
 * @file radiohound_dma_api.h
 * @author Thomas Pickle
 * @brief High-level API for RadioHound DMA operations
 * @version 0.2
 * @date 2024-07-10
 *
 * @copyright Copyright (c) 2024
 *
 *************************************************************************************************/
#ifndef RADIOHOUND_DMA_API_H_
#define RADIOHOUND_DMA_API_H_

#include <stdint.h>

/**************************************************************************************************
 * @brief Initializes the DMA system for RadioHound.
 *************************************************************************************************/
void rh_dma_init(void);

/**************************************************************************************************
 * @brief Runs a full memory-to-memory ping-pong DMA test.
 *
 * This function will:
 * 1. Generate test data in source buffers.
 * 2. Configure the DMA descriptors for a cyclic ping-pong transfer.
 * 3. Start the transfer and manage the chain for a set number of loops.
 * 4. Verify the data in the destination buffers against the source.
 *
 * @param num_transfers The total number of buffer transfers to perform.
 * @return int 0 on success, non-zero on failure.
 *************************************************************************************************/
int rh_dma_run_m2m_ping_pong_test(int num_transfers);

/**************************************************************************************************
 * @brief Generates predictable test data in a buffer.
 *
 * @param buffer Pointer to the buffer to fill.
 * @param size The size of the buffer in bytes.
 * @param seed A seed value to vary the data pattern.
 *************************************************************************************************/
void rh_generate_test_data(uint8_t* buffer, size_t size, uint8_t seed);

/**************************************************************************************************
 * @brief Verifies the contents of a destination buffer against an expected source.
 *
 * @param expected Pointer to the buffer with the correct data.
 * @param actual Pointer to the buffer that was transferred.
 * @param size The size of the buffers in bytes.
 * @param buffer_num An identifier for the buffer being checked, for logging.
 * @return int 1 on success (match), 0 on failure (mismatch).
 *************************************************************************************************/
int rh_verify_data_transfer(uint8_t* expected, uint8_t* actual, size_t size, int buffer_num);


#endif /* RADIOHOUND_DMA_API_H_ */
