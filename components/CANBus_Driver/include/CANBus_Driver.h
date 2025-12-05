#ifdef __cplusplus
extern "C" {
#endif

#include "driver/twai.h"

#define CAN_TX_GPIO     (gpio_num_t)21
#define CAN_RX_GPIO     (gpio_num_t)22
#define CANBUS_SPEED    500000   // 500kbps

#define CAN_QUEUE_LENGTH 32
#define CAN_QUEUE_ITEM_SIZE sizeof(twai_message_t)
#define TAG "TWAI"

extern bool receiving_data;
extern void (*can_message_handler)(twai_message_t *message);

void canbus_init(void);
void start_can_tasks(void);

#ifdef __cplusplus
}
#endif