#ifndef ADC_DEMO_H_
#define ADC_DEMO_H_

#include <zephyr/kernel.h>

/* 初始化 ADC：返回 0 表示成功 */
int adc_demo_init(void);

/* 测量一次：成功返回 raw 值（0~4095），失败返回负数 errno */
int adc_demo_read(void);
int adc_demo_read2(void); 

#endif /* ADC_DEMO_H_ */
