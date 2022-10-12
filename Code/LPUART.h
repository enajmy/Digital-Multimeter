/* ---- LPUART.h ---- */
#ifndef SRC_LPUART_H_
#define SRC_LPUART_H_

// Include #defines
#define LPUART GPIOG
#define BAUD_RATE_HEX 53333
#define ESC 0x1B


// Include function definitions / prototypes
void LPUART_init(void);
void LPUART_write_ESC(void);
void LPUART_print(const char *str);
void LPUART_ESC_Code(const char *str);

#endif /* SRC_LPUART_H_ */
