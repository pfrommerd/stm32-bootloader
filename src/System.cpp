#include "System.hpp"

#include <stm32f7xx_hal.h>

namespace bootloader { namespace system {
    void partial_init() {
        HAL_Init();
        __HAL_RCC_PWR_CLK_ENABLE(); // Lets us access the rtc backup registers
    }
    void full_init() {
        RCC_OscInitTypeDef RCC_OscInitStruct;
        RCC_ClkInitTypeDef RCC_ClkInitStruct;

        /*Configure the main internal regulator output voltage*/

        __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

        /*Initializes the CPU, AHB and APB busses clocks */
        RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
        RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
        RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
        RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
        RCC_OscInitStruct.PLL.PLLM = 4;
        RCC_OscInitStruct.PLL.PLLN = 216;
        RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
        RCC_OscInitStruct.PLL.PLLQ = 9;
        #if defined (RCC_PLLCFGR_PLLR)
            RCC_OscInitStruct.PLL.PLLR = 2;
        #endif
        if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
            asm("bkpt 255");
        }

        /*Activate the Over-Drive mode*/
        if (HAL_PWREx_EnableOverDrive() != HAL_OK) {
            asm("bkpt 255");
        }

        /*Initializes the CPU, AHB and APB busses clocks*/
        RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                      | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
        RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
        RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
        RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
        RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

        if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK) {
            asm("bkpt 255");
        }

        FLASH->ACR |= FLASH_ACR_PRFTEN;

        SystemCoreClockUpdate();

        /*Configure the Systick interrupt time*/
        if (HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000) != 0) {
            asm("bkpt 255");
        }

        /*Configure the Systick*/
        HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

        HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

        /* System interrupt init*/
        /* MemoryManagement_IRQn interrupt configuration */
        HAL_NVIC_SetPriority(MemoryManagement_IRQn, 0, 0);
        /* BusFault_IRQn interrupt configuration */
        HAL_NVIC_SetPriority(BusFault_IRQn, 0, 0);
        /* UsageFault_IRQn interrupt configuration */
        HAL_NVIC_SetPriority(UsageFault_IRQn, 0, 0);
        /* SVCall_IRQn interrupt configuration */
        HAL_NVIC_SetPriority(SVCall_IRQn, 0, 0);
        /* DebugMonitor_IRQn interrupt configuration */
        HAL_NVIC_SetPriority(DebugMonitor_IRQn, 0, 0);
        /* PendSV_IRQn interrupt configuration */
        HAL_NVIC_SetPriority(PendSV_IRQn, 0, 0);
        /* SysTick_IRQn interrupt configuration */
        HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);

		SCB_EnableICache();

		__GPIOA_CLK_ENABLE();
		__GPIOB_CLK_ENABLE();
		__GPIOC_CLK_ENABLE();
		__GPIOD_CLK_ENABLE();
		__GPIOE_CLK_ENABLE();
		__GPIOF_CLK_ENABLE();
		__GPIOG_CLK_ENABLE();
		__GPIOH_CLK_ENABLE();
		__GPIOI_CLK_ENABLE();
		__GPIOJ_CLK_ENABLE();
		__GPIOK_CLK_ENABLE();

		__HAL_RCC_DMA1_CLK_ENABLE();
		__HAL_RCC_DMA2_CLK_ENABLE();

		//Disable everything when a breakpoint occurs, except the RTC
		DBGMCU->APB1FZ = 0xFFFFFFFF & ~(1 << 10);
		DBGMCU->APB2FZ = 0xFFFFFFFF;

		//Connect CPU core lockup to the advanced timer break inputs
		#if defined(STM32F76xx) || defined(STM32F77xx)
		SYSCFG->CBR |= SYSCFG_CBR_CLL;
		#endif

		//Enable the clock security system
		HAL_RCC_EnableCSS();
    }
    void deinit() {
        //Disable SysTick and clear its exception pending bit
        SysTick->CTRL = 0 ;
        SysTick->LOAD = 0;
        SysTick->VAL  = 0;

        SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk ;

        //Disable individual fault handlers
        SCB->SHCSR &= ~( SCB_SHCSR_USGFAULTENA_Msk | \
                     SCB_SHCSR_BUSFAULTENA_Msk | \
                     SCB_SHCSR_MEMFAULTENA_Msk ) ;
        //Activate the MSP, if the core is found to currently run with the PSP.
        if( CONTROL_SPSEL_Msk & __get_CONTROL( ) )
        {  /* MSP is not active */
            __set_CONTROL( __get_CONTROL( ) & ~CONTROL_SPSEL_Msk ) ;
        }

        HAL_RCC_DeInit();
        HAL_DeInit();
    }
    void run(void* app) {
        uint32_t* addr = (uint32_t*) app;
        if(CONTROL_nPRIV_Msk & __get_CONTROL( )) {}

        deinit();

        //Load the vector table address of the user application into SCB->VTOR register
        SCB->VTOR = ( uint32_t )addr;

        //Set the MSP to the value found in the user application vector table
        __set_MSP( addr[ 0 ] ) ;

        //Set the PC to the reset vector value of the user application via a function call
        ( ( void ( * )( void ) )addr[1] )( ) ;
    }

    extern "C" {
        void SysTick_Handler() {
            HAL_IncTick();
            HAL_SYSTICK_IRQHandler();
        }

        void HAL_SYSTICK_Callback() {}

        void RCC_IRQHandler() {
            HAL_RCC_NMI_IRQHandler();
        }
        void HAL_RCC_CSSCallback() {}
    }
}}
