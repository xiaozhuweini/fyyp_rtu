#ifndef __DRV_PIN_H__
#define __DRV_PIN_H__



#include "rtthread.h"



#define PIN_LQFP_100
//#define PIN_LQFP_144
#define PIN_MODE_AF_EX



#define PIN_STA_LOW								0
#define PIN_STA_HIGH							1

#define PIN_MODE_O_PP_NOPULL					0
#define PIN_MODE_O_PP_PULLUP					1
#define PIN_MODE_O_PP_PULLDOWN					2
#define PIN_MODE_O_OD_NOPULL					3
#define PIN_MODE_O_OD_PULLUP					4
#define PIN_MODE_O_OD_PULLDOWN					5
#define PIN_MODE_I_NOPULL						6
#define PIN_MODE_I_PULLUP						7
#define PIN_MODE_I_PULLDOWN						8
#define PIN_MODE_ANALOG							9
#define PIN_MODE_AF_PP_NOPULL					10
#define PIN_MODE_AF_PP_PULLUP					11
#define PIN_MODE_AF_PP_PULLDOWN					12
#define PIN_MODE_AF_OD_NOPULL					13
#define PIN_MODE_AF_OD_PULLUP					14
#define PIN_MODE_AF_OD_PULLDOWN					15

#define PIN_IRQ_MODE_RISING						0
#define PIN_IRQ_MODE_FALLING					1
#define PIN_IRQ_MODE_RISING_FALLING				2

#define PIN_PORT_A								(1 << 8)
#define PIN_PORT_B								(2 << 8)
#define PIN_PORT_C								(3 << 8)
#define PIN_PORT_D								(4 << 8)
#define PIN_PORT_E								(5 << 8)
#define PIN_PORT_F								(6 << 8)
#define PIN_PORT_G								(7 << 8)
#define PIN_PORT_H								(8 << 8)

#ifdef PIN_LQFP_100

#define PIN_INDEX_23							(PIN_PORT_A + 0)
#define PIN_INDEX_24							(PIN_PORT_A + 1)
#define PIN_INDEX_25							(PIN_PORT_A + 2)
#define PIN_INDEX_26							(PIN_PORT_A + 3)
#define PIN_INDEX_29							(PIN_PORT_A + 4)
#define PIN_INDEX_30							(PIN_PORT_A + 5)
#define PIN_INDEX_31							(PIN_PORT_A + 6)
#define PIN_INDEX_32							(PIN_PORT_A + 7)
#define PIN_INDEX_67							(PIN_PORT_A + 8)
#define PIN_INDEX_68							(PIN_PORT_A + 9)
#define PIN_INDEX_69							(PIN_PORT_A + 10)
#define PIN_INDEX_70							(PIN_PORT_A + 11)
#define PIN_INDEX_71							(PIN_PORT_A + 12)
#define PIN_INDEX_72							(PIN_PORT_A + 13)
#define PIN_INDEX_76							(PIN_PORT_A + 14)
#define PIN_INDEX_77							(PIN_PORT_A + 15)

#define PIN_INDEX_35							(PIN_PORT_B + 0)
#define PIN_INDEX_36							(PIN_PORT_B + 1)
#define PIN_INDEX_37							(PIN_PORT_B + 2)
#define PIN_INDEX_89							(PIN_PORT_B + 3)
#define PIN_INDEX_90							(PIN_PORT_B + 4)
#define PIN_INDEX_91							(PIN_PORT_B + 5)
#define PIN_INDEX_92							(PIN_PORT_B + 6)
#define PIN_INDEX_93							(PIN_PORT_B + 7)
#define PIN_INDEX_95							(PIN_PORT_B + 8)
#define PIN_INDEX_96							(PIN_PORT_B + 9)
#define PIN_INDEX_47							(PIN_PORT_B + 10)
#define PIN_INDEX_48							(PIN_PORT_B + 11)
#define PIN_INDEX_51							(PIN_PORT_B + 12)
#define PIN_INDEX_52							(PIN_PORT_B + 13)
#define PIN_INDEX_53							(PIN_PORT_B + 14)
#define PIN_INDEX_54							(PIN_PORT_B + 15)

#define PIN_INDEX_15							(PIN_PORT_C + 0)
#define PIN_INDEX_16							(PIN_PORT_C + 1)
#define PIN_INDEX_17							(PIN_PORT_C + 2)
#define PIN_INDEX_18							(PIN_PORT_C + 3)
#define PIN_INDEX_33							(PIN_PORT_C + 4)
#define PIN_INDEX_34							(PIN_PORT_C + 5)
#define PIN_INDEX_63							(PIN_PORT_C + 6)
#define PIN_INDEX_64							(PIN_PORT_C + 7)
#define PIN_INDEX_65							(PIN_PORT_C + 8)
#define PIN_INDEX_66							(PIN_PORT_C + 9)
#define PIN_INDEX_78							(PIN_PORT_C + 10)
#define PIN_INDEX_79							(PIN_PORT_C + 11)
#define PIN_INDEX_80							(PIN_PORT_C + 12)
#define PIN_INDEX_7								(PIN_PORT_C + 13)
#define PIN_INDEX_8								(PIN_PORT_C + 14)
#define PIN_INDEX_9								(PIN_PORT_C + 15)

#define PIN_INDEX_81							(PIN_PORT_D + 0)
#define PIN_INDEX_82							(PIN_PORT_D + 1)
#define PIN_INDEX_83							(PIN_PORT_D + 2)
#define PIN_INDEX_84							(PIN_PORT_D + 3)
#define PIN_INDEX_85							(PIN_PORT_D + 4)
#define PIN_INDEX_86							(PIN_PORT_D + 5)
#define PIN_INDEX_87							(PIN_PORT_D + 6)
#define PIN_INDEX_88							(PIN_PORT_D + 7)
#define PIN_INDEX_55							(PIN_PORT_D + 8)
#define PIN_INDEX_56							(PIN_PORT_D + 9)
#define PIN_INDEX_57							(PIN_PORT_D + 10)
#define PIN_INDEX_58							(PIN_PORT_D + 11)
#define PIN_INDEX_59							(PIN_PORT_D + 12)
#define PIN_INDEX_60							(PIN_PORT_D + 13)
#define PIN_INDEX_61							(PIN_PORT_D + 14)
#define PIN_INDEX_62							(PIN_PORT_D + 15)

#define PIN_INDEX_97							(PIN_PORT_E + 0)
#define PIN_INDEX_98							(PIN_PORT_E + 1)
#define PIN_INDEX_1								(PIN_PORT_E + 2)
#define PIN_INDEX_2								(PIN_PORT_E + 3)
#define PIN_INDEX_3								(PIN_PORT_E + 4)
#define PIN_INDEX_4								(PIN_PORT_E + 5)
#define PIN_INDEX_5								(PIN_PORT_E + 6)
#define PIN_INDEX_38							(PIN_PORT_E + 7)
#define PIN_INDEX_39							(PIN_PORT_E + 8)
#define PIN_INDEX_40							(PIN_PORT_E + 9)
#define PIN_INDEX_41							(PIN_PORT_E + 10)
#define PIN_INDEX_42							(PIN_PORT_E + 11)
#define PIN_INDEX_43							(PIN_PORT_E + 12)
#define PIN_INDEX_44							(PIN_PORT_E + 13)
#define PIN_INDEX_45							(PIN_PORT_E + 14)
#define PIN_INDEX_46							(PIN_PORT_E + 15)

#define PIN_INDEX_12							(PIN_PORT_H + 0)
#define PIN_INDEX_13							(PIN_PORT_H + 1)

#endif

#ifdef PIN_LQFP_144

#define PIN_INDEX_34							(PIN_PORT_A + 0)
#define PIN_INDEX_35							(PIN_PORT_A + 1)
#define PIN_INDEX_36							(PIN_PORT_A + 2)
#define PIN_INDEX_37							(PIN_PORT_A + 3)
#define PIN_INDEX_40							(PIN_PORT_A + 4)
#define PIN_INDEX_41							(PIN_PORT_A + 5)
#define PIN_INDEX_42							(PIN_PORT_A + 6)
#define PIN_INDEX_43							(PIN_PORT_A + 7)
#define PIN_INDEX_100							(PIN_PORT_A + 8)
#define PIN_INDEX_101							(PIN_PORT_A + 9)
#define PIN_INDEX_102							(PIN_PORT_A + 10)
#define PIN_INDEX_103							(PIN_PORT_A + 11)
#define PIN_INDEX_104							(PIN_PORT_A + 12)
#define PIN_INDEX_105							(PIN_PORT_A + 13)
#define PIN_INDEX_109							(PIN_PORT_A + 14)
#define PIN_INDEX_110							(PIN_PORT_A + 15)

#define PIN_INDEX_46							(PIN_PORT_B + 0)
#define PIN_INDEX_47							(PIN_PORT_B + 1)
#define PIN_INDEX_48							(PIN_PORT_B + 2)
#define PIN_INDEX_133							(PIN_PORT_B + 3)
#define PIN_INDEX_134							(PIN_PORT_B + 4)
#define PIN_INDEX_135							(PIN_PORT_B + 5)
#define PIN_INDEX_136							(PIN_PORT_B + 6)
#define PIN_INDEX_137							(PIN_PORT_B + 7)
#define PIN_INDEX_139							(PIN_PORT_B + 8)
#define PIN_INDEX_140							(PIN_PORT_B + 9)
#define PIN_INDEX_69							(PIN_PORT_B + 10)
#define PIN_INDEX_70							(PIN_PORT_B + 11)
#define PIN_INDEX_73							(PIN_PORT_B + 12)
#define PIN_INDEX_74							(PIN_PORT_B + 13)
#define PIN_INDEX_75							(PIN_PORT_B + 14)
#define PIN_INDEX_76							(PIN_PORT_B + 15)

#define PIN_INDEX_26							(PIN_PORT_C + 0)
#define PIN_INDEX_27							(PIN_PORT_C + 1)
#define PIN_INDEX_28							(PIN_PORT_C + 2)
#define PIN_INDEX_29							(PIN_PORT_C + 3)
#define PIN_INDEX_44							(PIN_PORT_C + 4)
#define PIN_INDEX_45							(PIN_PORT_C + 5)
#define PIN_INDEX_96							(PIN_PORT_C + 6)
#define PIN_INDEX_97							(PIN_PORT_C + 7)
#define PIN_INDEX_98							(PIN_PORT_C + 8)
#define PIN_INDEX_99							(PIN_PORT_C + 9)
#define PIN_INDEX_111							(PIN_PORT_C + 10)
#define PIN_INDEX_112							(PIN_PORT_C + 11)
#define PIN_INDEX_113							(PIN_PORT_C + 12)
#define PIN_INDEX_7								(PIN_PORT_C + 13)
#define PIN_INDEX_8								(PIN_PORT_C + 14)
#define PIN_INDEX_9								(PIN_PORT_C + 15)

#define PIN_INDEX_114							(PIN_PORT_D + 0)
#define PIN_INDEX_115							(PIN_PORT_D + 1)
#define PIN_INDEX_116							(PIN_PORT_D + 2)
#define PIN_INDEX_117							(PIN_PORT_D + 3)
#define PIN_INDEX_118							(PIN_PORT_D + 4)
#define PIN_INDEX_119							(PIN_PORT_D + 5)
#define PIN_INDEX_122							(PIN_PORT_D + 6)
#define PIN_INDEX_123							(PIN_PORT_D + 7)
#define PIN_INDEX_77							(PIN_PORT_D + 8)
#define PIN_INDEX_78							(PIN_PORT_D + 9)
#define PIN_INDEX_79							(PIN_PORT_D + 10)
#define PIN_INDEX_80							(PIN_PORT_D + 11)
#define PIN_INDEX_81							(PIN_PORT_D + 12)
#define PIN_INDEX_82							(PIN_PORT_D + 13)
#define PIN_INDEX_85							(PIN_PORT_D + 14)
#define PIN_INDEX_86							(PIN_PORT_D + 15)

#define PIN_INDEX_141							(PIN_PORT_E + 0)
#define PIN_INDEX_142							(PIN_PORT_E + 1)
#define PIN_INDEX_1								(PIN_PORT_E + 2)
#define PIN_INDEX_2								(PIN_PORT_E + 3)
#define PIN_INDEX_3								(PIN_PORT_E + 4)
#define PIN_INDEX_4								(PIN_PORT_E + 5)
#define PIN_INDEX_5								(PIN_PORT_E + 6)
#define PIN_INDEX_58							(PIN_PORT_E + 7)
#define PIN_INDEX_59							(PIN_PORT_E + 8)
#define PIN_INDEX_60							(PIN_PORT_E + 9)
#define PIN_INDEX_63							(PIN_PORT_E + 10)
#define PIN_INDEX_64							(PIN_PORT_E + 11)
#define PIN_INDEX_65							(PIN_PORT_E + 12)
#define PIN_INDEX_66							(PIN_PORT_E + 13)
#define PIN_INDEX_67							(PIN_PORT_E + 14)
#define PIN_INDEX_68							(PIN_PORT_E + 15)

#define PIN_INDEX_10							(PIN_PORT_F + 0)
#define PIN_INDEX_11							(PIN_PORT_F + 1)
#define PIN_INDEX_12							(PIN_PORT_F + 2)
#define PIN_INDEX_13							(PIN_PORT_F + 3)
#define PIN_INDEX_14							(PIN_PORT_F + 4)
#define PIN_INDEX_15							(PIN_PORT_F + 5)
#define PIN_INDEX_18							(PIN_PORT_F + 6)
#define PIN_INDEX_19							(PIN_PORT_F + 7)
#define PIN_INDEX_20							(PIN_PORT_F + 8)
#define PIN_INDEX_21							(PIN_PORT_F + 9)
#define PIN_INDEX_22							(PIN_PORT_F + 10)
#define PIN_INDEX_49							(PIN_PORT_F + 11)
#define PIN_INDEX_50							(PIN_PORT_F + 12)
#define PIN_INDEX_53							(PIN_PORT_F + 13)
#define PIN_INDEX_54							(PIN_PORT_F + 14)
#define PIN_INDEX_55							(PIN_PORT_F + 15)

#define PIN_INDEX_56							(PIN_PORT_G + 0)
#define PIN_INDEX_57							(PIN_PORT_G + 1)
#define PIN_INDEX_87							(PIN_PORT_G + 2)
#define PIN_INDEX_88							(PIN_PORT_G + 3)
#define PIN_INDEX_89							(PIN_PORT_G + 4)
#define PIN_INDEX_90							(PIN_PORT_G + 5)
#define PIN_INDEX_91							(PIN_PORT_G + 6)
#define PIN_INDEX_92							(PIN_PORT_G + 7)
#define PIN_INDEX_93							(PIN_PORT_G + 8)
#define PIN_INDEX_124							(PIN_PORT_G + 9)
#define PIN_INDEX_125							(PIN_PORT_G + 10)
#define PIN_INDEX_126							(PIN_PORT_G + 11)
#define PIN_INDEX_127							(PIN_PORT_G + 12)
#define PIN_INDEX_128							(PIN_PORT_G + 13)
#define PIN_INDEX_129							(PIN_PORT_G + 14)
#define PIN_INDEX_132							(PIN_PORT_G + 15)

#define PIN_INDEX_23							(PIN_PORT_H + 0)
#define PIN_INDEX_24							(PIN_PORT_H + 1)

#endif



#define _pin_get_st_pin(pin_index)				(1 << (pin_index & 0xff))



typedef void (*PIN_FUN_IRQ_HDR)(void *args);

typedef struct pin_irq_hdr
{
    rt_uint16_t		pin;
    rt_uint16_t		mode;
    PIN_FUN_IRQ_HDR	hdr;
    void			*args;
} PIN_IRQ_HDR;



void pin_clock_enable(rt_uint16_t pin_index, rt_uint8_t en);
void pin_write(rt_uint16_t pin_index, rt_uint8_t pin_sta);
void pin_toggle(rt_uint16_t pin_index);
rt_uint8_t pin_read(rt_uint16_t pin_index);
void pin_mode(rt_uint16_t pin_index, rt_uint8_t mode);
#ifdef PIN_MODE_AF_EX
void pin_mode_ex(rt_uint16_t pin_index, rt_uint8_t mode, rt_uint8_t af_index);
#endif
rt_uint8_t pin_attach_irq(rt_uint16_t pin_index, rt_uint8_t irq_mode, PIN_FUN_IRQ_HDR hdr, void *args);
rt_uint8_t pin_detach_irq(rt_uint16_t pin_index);
rt_uint8_t pin_irq_enable(rt_uint16_t pin_index, rt_uint8_t en);



#endif
