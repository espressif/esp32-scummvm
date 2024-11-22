#include "hid_ev.h"

#ifdef __cplusplus
extern "C" {
#endif

void usb_hid_task();
int usb_hid_receive_hid_event(hid_ev_t *ev);

#ifdef __cplusplus
}
#endif
