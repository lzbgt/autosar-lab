#ifndef LAB_INTERNAL_H
#define LAB_INTERNAL_H

#include "lab.h"

void lab_application_run_10ms(LabEcu *ecu);
void lab_rte_com_publish_speed(LabEcu *ecu);
int lab_pdur_canif_transmit(LabEcu *ecu, uint32_t can_id, const uint8_t *payload, size_t len,
                            bool fd);
void lab_cantp_rx_indication(LabEcu *ecu, const LabFrame *frame);
void lab_cantp_main_function(LabEcu *ecu);
int lab_cantp_transmit(LabEcu *ecu, const uint8_t *payload, size_t len);
void lab_dcm_request(LabEcu *ecu, const uint8_t *request, size_t len);

#endif
