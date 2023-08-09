
/* Includes ------------------------------------------------------------------*/
#include "alt_main.h"

/* Private includes ----------------------------------------------------------*/
#include "monitor.hpp"
#include "sharp_display.h"
#include <Temperature_LM75_Derived.h>
#include "bitmaps.h"
#include "gpio.h"
#include "spi.h"
#include "rtc.h"
#include "main.h"


/* Private typedef -----------------------------------------------------------*/

typedef struct
{
	uint16_t Pin;
	GPIO_PinState Value;
	uint32_t Time;
} SwitchTime_t;

typedef struct {
	uint8_t hours;
	uint8_t minutes;
	uint8_t seconds;
} TOD_t;

enum class State_t { STOPPED, RUNNING, SETUP };

enum class Command_t { LEFT_SHORT, LEFT_LONG, RIGHT_SHORT, RIGHT_LONG };

enum class Display_t { RIDE, INFO };

enum class SetUpScreen_t { MAIN, CLOCK, ODO, WHEEL };

/* Private define ------------------------------------------------------------*/

#define USE_SLEEP	1

#define SHORT_MIN	100	// mS
#define SHORT_MAX	800
#define LONG_MIN	1000
#define LONG_MAX	3500

#define CHECK			0xAB200723
#define CHECK_ADR		0x08080000
#define WHEEL_ADR		(CHECK_ADR + 4)
#define ODOMETER_ADR	(CHECK_ADR + 8)

#define readFromEEPROMi16(address)		(*(__IO int16_t *)(address))
#define readFromEEPROMu32(address)		(*(__IO uint32_t *)(address))

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
cMonitor monitor;
sharp_display display(SPI1_CS_GPIO_Port, SPI1_CS_Pin, &hspi1);
Generic_LM75_11Bit temperature(&hi2c1);

volatile State_t systemState = State_t::STOPPED;

#if 0
volatile SwitchTime_t controlQ[16];
volatile uint8_t controlH = 0, controlT = 0;
#else
GPIO_PinState leftLastState = GPIO_PIN_SET;
GPIO_PinState rightLastState = GPIO_PIN_SET;
#endif
volatile SwitchTime_t sensorQ[16];
volatile uint8_t sensorH = 0, sensorT = 0;
volatile uint32_t lastCqTime = 0;

volatile uint32_t Distance = 0;
volatile uint32_t SpdLastTime = 0;
volatile uint32_t Speed = 0;
volatile uint32_t MaximumSpeed = 0;
volatile uint32_t CadLastTime = 0;
volatile uint32_t Cadence = 0;
volatile uint32_t Duration = 0;
volatile uint32_t Odometer = 0;

volatile uint32_t leftStartTime = 0;
volatile uint32_t rightStartTime = 0;

volatile Display_t currentDisplay = Display_t::RIDE;

volatile SetUpScreen_t setupScreen = SetUpScreen_t::MAIN;
volatile uint8_t setupSelection = 0;

volatile int16_t wheelDiameter = 2125;

char clockText[] = "24:00";
char OdometerText[] = "0000.0";
char wheelDiameterText[] = "2125";

/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/

void drawSpeed(int16_t x, int16_t y)
{
	display.drawBitmap(&x, y, &bitmaps32[(Speed/100)%10]);
	display.drawBitmap(&x, y, &bitmaps32[(Speed/10)%10]);
	display.drawBitmap(&x, y, &bitmaps32[period_idx]);
	display.drawBitmap(&x, y, &bitmaps32[(Speed/1)%10]);
}

void drawMaximumSpeed(int16_t x, int16_t y)
{
	display.drawBitmap(&x, y, &bitmaps32[max_idx]);
	display.drawBitmap(&x, y, &bitmaps32[(MaximumSpeed/100)%10]);
	display.drawBitmap(&x, y, &bitmaps32[(MaximumSpeed/10)%10]);
	display.drawBitmap(&x, y, &bitmaps32[period_idx]);
	display.drawBitmap(&x, y, &bitmaps32[(MaximumSpeed/1)%10]);
}

void drawCadence(int16_t x, int16_t y)
{
	display.drawBitmap(&x, y, &bitmaps32[cad_idx]);
	uint32_t d = (Cadence/100)%10;
	if (d != 0)
		display.drawBitmap(&x, y, &bitmaps32[d]);
	d = (Cadence/10)%10;
	if (d != 0)
		display.drawBitmap(&x, y, &bitmaps32[d]);
	display.drawBitmap(&x, y, &bitmaps32[(Cadence/1)%10]);
}

void drawDistance(int16_t x, int16_t y)
{
	display.drawBitmap(&x, y, &bitmaps32[dst_idx]);
	uint32_t dist = Distance*wheelDiameter/10000;
	uint32_t d = (dist/10000)%10;
	if (d != 0)
		display.drawBitmap(&x, y, &bitmaps32[d]);
	d = (dist/1000)%10;
	if (d != 0)
		display.drawBitmap(&x, y, &bitmaps32[d]);
	display.drawBitmap(&x, y, &bitmaps32[(dist/100)%10]);
	display.drawBitmap(&x, y, &bitmaps32[period_idx]);
	display.drawBitmap(&x, y, &bitmaps32[(dist/10)%10]);
	display.drawBitmap(&x, y, &bitmaps32[(dist/1)%10]);
}

void drawDuration(int16_t x, int16_t y)
{
	int t = Duration / 1000;
	int s = t % 60;
	t = t / 60;
	int m = t % 60;
	int h = t / 60;

	display.drawBitmap(&x, y, &bitmaps32[(h/10)%10]);
	display.drawBitmap(&x, y, &bitmaps32[h%10]);
	display.drawBitmap(&x, y, &bitmaps32[colon_idx]);
	display.drawBitmap(&x, y, &bitmaps32[(m/10)%10]);
	display.drawBitmap(&x, y, &bitmaps32[m%10]);
	display.drawBitmap(&x, y, &bitmaps32[colon_idx]);
	display.drawBitmap(&x, y, &bitmaps32[(s/10)%10]);
	display.drawBitmap(&x, y, &bitmaps32[s%10]);
}

void drawTime(int16_t x, int16_t y)
{
	RTC_DateTypeDef sdatestructureget;
	RTC_TimeTypeDef stimestructureget;
	HAL_RTC_GetTime(&hrtc, &stimestructureget, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc, &sdatestructureget, RTC_FORMAT_BIN);
	display.drawBitmap(&x, y, &bitmaps32[(stimestructureget.Hours/10)%10]);
	display.drawBitmap(&x, y, &bitmaps32[stimestructureget.Hours%10]);
	display.drawBitmap(&x, y, &bitmaps32[colon_idx]);
	display.drawBitmap(&x, y, &bitmaps32[(stimestructureget.Minutes/10)%10]);
	display.drawBitmap(&x, y, &bitmaps32[stimestructureget.Minutes%10]);
}

void drawTemperature(int16_t x, int16_t y)
{
	temperature.disableShutdownMode();
	HAL_Delay(10);
	float t = temperature.readTemperatureC();
	temperature.enableShutdownMode();

	if (t < 0)
		t = -t;
	display.drawBitmap(&x, y, &bitmaps32[((int)t/10)%10]);
	display.drawBitmap(&x, y, &bitmaps32[(int)t%10]);
	display.drawBitmap(&x, y, &bitmaps32[period_idx]);
	display.drawBitmap(&x, y, &bitmaps32[(int)(t*10)%10]);
	display.drawBitmap(&x, y, &bitmaps32[degree_idx]);
}

void drawOdometer(int16_t x, int16_t y)
{
	display.drawBitmap(&x, y, &bitmaps32[odo_idx]);
	uint32_t dist = Odometer*wheelDiameter/100000;
	uint32_t d = (dist/10000)%10;
	if (d != 0)
		display.drawBitmap(&x, y, &bitmaps32[d]);
	d = (dist/1000)%10;
	if (d != 0)
		display.drawBitmap(&x, y, &bitmaps32[d]);
	d = (dist/100)%10;
	if (d != 0)
		display.drawBitmap(&x, y, &bitmaps32[d]);
	display.drawBitmap(&x, y, &bitmaps32[(dist/10)%10]);
	display.drawBitmap(&x, y, &bitmaps32[period_idx]);
	display.drawBitmap(&x, y, &bitmaps32[(dist/1)%10]);
}

void DisplaySetup()
{
	switch (setupScreen) {
	case SetUpScreen_t::MAIN:
		display.clearDisplay();
		display.drawBitmap(0, setupSelection*32, bitmaps32[rarrow_idx].bitmap, 18, 32, 0);
		display.drawString(18, 0, bitmaps32, (char *)"CLOCK");
		display.drawString(18, 32, bitmaps32, (char *)"ODO");
		display.drawString(18, 64, bitmaps32, (char *)"WHEEL");
		display.refresh();
		break;
	case SetUpScreen_t::CLOCK: {
		RTC_DateTypeDef sdatestructureget;
		RTC_TimeTypeDef stimestructureget;
		HAL_RTC_GetTime(&hrtc, &stimestructureget, RTC_FORMAT_BIN);
		HAL_RTC_GetDate(&hrtc, &sdatestructureget, RTC_FORMAT_BIN);
		clockText[0] = '0' + stimestructureget.Hours / 10;
		clockText[1] = '0' + stimestructureget.Hours % 10;
		clockText[2] = ':';
		clockText[3] = '0' + stimestructureget.Minutes / 10;
		clockText[4] = '0' + stimestructureget.Minutes % 10;
		display.clearDisplay();
		display.drawString(0, 0, bitmaps32, (char *)"CLOCK");
		display.drawString(0, 32, bitmaps32, clockText);
		display.drawBitmap(setupSelection*18, 64, bitmaps32[uarrow_idx].bitmap, 18, 32, 0);
		display.refresh();
	} break;
	case SetUpScreen_t::ODO: {
		uint32_t odo = Odometer*wheelDiameter/100000;
		OdometerText[0] = '0' + odo/10000;
		OdometerText[1] = '0' + (odo/1000) % 10;
		OdometerText[2] = '0' + (odo/100) % 10;
		OdometerText[3] = '0' + (odo/10) % 10;
		OdometerText[4] = '.';
		OdometerText[5] = '0' + odo % 10;
		display.clearDisplay();
		display.drawString(0, 0, bitmaps32, (char *)"ODO");
		display.drawString(0, 32, bitmaps32, OdometerText);
		display.drawBitmap(setupSelection*18, 64, bitmaps32[uarrow_idx].bitmap, 18, 32, 0);
		display.refresh();
	} break;
	case SetUpScreen_t::WHEEL:
		wheelDiameterText[0] = '0' + wheelDiameter/1000;
		wheelDiameterText[1] = '0' + (wheelDiameter/100) % 10;
		wheelDiameterText[2] = '0' + (wheelDiameter/10) % 10;
		wheelDiameterText[3] = '0' + wheelDiameter % 10;
		display.clearDisplay();
		display.drawString(0, 0, bitmaps32, (char *)"WHEEL");
		display.drawString(0, 32, bitmaps32, wheelDiameterText);
		display.drawBitmap(setupSelection*18, 64, bitmaps32[uarrow_idx].bitmap, 18, 32, 0);
		display.refresh();
		break;
	}
}

void CommandProcess(Command_t cmd)
{
	if (systemState != State_t::SETUP) {
		if (cmd == Command_t::RIGHT_LONG) {
			// reset time and CadCount
			Distance = 0;
			MaximumSpeed = 0;
			Duration = 0;
			monitor.schedule((char*)"DisplayUpdate", 0);
		} else if (cmd == Command_t::LEFT_SHORT) {
			if (currentDisplay == Display_t::RIDE)
				currentDisplay = Display_t::INFO;
			else
				currentDisplay = Display_t::RIDE;
			monitor.schedule((char*)"DisplayUpdate", 0);
		} else if (cmd == Command_t::LEFT_LONG) {
			systemState = State_t::SETUP;
			monitor.schedule((char*)"DisplayUpdate", NEVER);
			setupScreen = SetUpScreen_t::MAIN;
			setupSelection = 0;
			DisplaySetup();
		} else if (cmd == Command_t::RIGHT_SHORT) {
		}
	} else {
		if (cmd == Command_t::RIGHT_SHORT) {
			if (setupScreen == SetUpScreen_t::MAIN) {
				if (setupSelection == 0)
					setupScreen = SetUpScreen_t::CLOCK;
				else if (setupSelection == 1)
					setupScreen = SetUpScreen_t::ODO;
				else if (setupSelection == 2)
					setupScreen = SetUpScreen_t::WHEEL;
				setupSelection = 0;
				DisplaySetup();
			} else if (setupScreen == SetUpScreen_t::CLOCK) {
				// Change clock displayed value
				if (setupSelection == 0) {
					clockText[0]++;
					if (clockText[0] == '2' + 1)
						clockText[0] = '0';
				} else if (setupSelection == 1) {
					clockText[1]++;
					if (clockText[0] != '2') {
						if (clockText[1] == '9' + 1)
							clockText[1] = '0';
					} else {
						if (clockText[1] == '3' + 1)
							clockText[1] = '0';
					}
				} else if (setupSelection == 2) {
					clockText[3]++;
					if (clockText[3] == '5' + 1)
						clockText[3] = '0';
				} else if (setupSelection == 3) {
					clockText[4]++;
					if (clockText[4] == '9' + 1)
						clockText[4] = '0';
				}
				display.clearBlock(setupSelection*18 + ((setupSelection>1)?9:0), 32, 18, 32);
				display.drawString(0, 32, bitmaps32, clockText);
				display.refresh();
			} else if (setupScreen == SetUpScreen_t::ODO) {
				// Change odometer displayed value
				int ts = setupSelection + (setupSelection==4 ? 1:0);
				OdometerText[ts]++;
				if (OdometerText[ts] == '9' + 1)
					OdometerText[ts] = '0';
				display.clearBlock(ts*18, 32, 18, 32);
				display.drawString(0, 32, bitmaps32, OdometerText);
				display.refresh();
			} else if (setupScreen == SetUpScreen_t::WHEEL) {
				// Change wheel diameter displayed value
				wheelDiameterText[setupSelection]++;
				if (wheelDiameterText[setupSelection] == '9' + 1)
					wheelDiameterText[setupSelection] = '0';
				display.clearBlock(setupSelection*18, 32, 18, 32);
				display.drawString(0, 32, bitmaps32, wheelDiameterText);
				display.refresh();
			}
		} else if (cmd == Command_t::RIGHT_LONG) {
			if (setupScreen == SetUpScreen_t::CLOCK) {
				// Update clock value from display
				RTC_DateTypeDef sdatestructureget;
				RTC_TimeTypeDef stimestructureget;
				HAL_RTC_GetTime(&hrtc, &stimestructureget, RTC_FORMAT_BIN);
				HAL_RTC_GetDate(&hrtc, &sdatestructureget, RTC_FORMAT_BIN);
				stimestructureget.Hours = (clockText[0] - '0') * 10 + (clockText[1] - '0');
				stimestructureget.Minutes = (clockText[3] - '0') * 10 + (clockText[4] - '0');
				HAL_RTC_SetTime(&hrtc, &stimestructureget, RTC_FORMAT_BIN);
			} else if (setupScreen == SetUpScreen_t::ODO) {
				// Update odometer value from display
				uint32_t odo = (OdometerText[0] - '0') * 10000
						+ (OdometerText[1] - '0') * 1000
						+ (OdometerText[2] - '0') * 100
						+ (OdometerText[3] - '0') * 10
						+ (OdometerText[5] - '0');
				Odometer = odo*100000/wheelDiameter;
			} else if (setupScreen == SetUpScreen_t::WHEEL) {
				// Update wheel diameter value from display
				wheelDiameter = (wheelDiameterText[0] - '0') * 1000
						+ (wheelDiameterText[1] - '0') * 100
						+ (wheelDiameterText[2] - '0') * 10
						+ (wheelDiameterText[3] - '0');
				if (HAL_FLASHEx_DATAEEPROM_Unlock() == HAL_OK) {
					HAL_FLASHEx_DATAEEPROM_Program(FLASH_TYPEPROGRAMDATA_HALFWORD, WHEEL_ADR, wheelDiameter);
					HAL_FLASHEx_DATAEEPROM_Lock();
				}
			}

			systemState = State_t::RUNNING;
			monitor.schedule((char*)"DisplayUpdate", 0);
		} else if (cmd == Command_t::LEFT_SHORT) {
			if (setupScreen == SetUpScreen_t::MAIN) {
				display.drawBitmap(0, setupSelection*32, bitmaps32[rarrow_idx].bitmap, 18, 32, 1);
				setupSelection = (setupSelection + 1) % 3;
				display.drawBitmap(0, setupSelection*32, bitmaps32[rarrow_idx].bitmap, 18, 32, 0);
				display.refresh();
			} else if (setupScreen == SetUpScreen_t::CLOCK) {
				int16_t p = setupSelection * 18 + (setupSelection>1 ? 9 : 0);
				display.drawBitmap(p, 64, bitmaps32[uarrow_idx].bitmap, 18, 32, 1);
				setupSelection = (setupSelection + 1) % 4;
				p = setupSelection * 18 + ((setupSelection > 1) ? 9 : 0);
				display.drawBitmap(p, 64, bitmaps32[uarrow_idx].bitmap, 18, 32, 0);
				display.refresh();
			} else if (setupScreen == SetUpScreen_t::ODO) {
				int16_t p = setupSelection * 18 + (setupSelection>3 ? 9 : 0);
				display.drawBitmap(p, 64, bitmaps32[uarrow_idx].bitmap, 18, 32, 1);
				setupSelection = (setupSelection + 1) % 5;
				p = setupSelection * 18 + (setupSelection>3 ? 9 : 0);
				display.drawBitmap(p, 64, bitmaps32[uarrow_idx].bitmap, 18, 32, 0);
				display.refresh();
			} else if (setupScreen == SetUpScreen_t::WHEEL) {
				display.drawBitmap(setupSelection*18, 64, bitmaps32[uarrow_idx].bitmap, 18, 32, 1);
				setupSelection = (setupSelection + 1) % 4;
				display.drawBitmap(setupSelection*18, 64, bitmaps32[uarrow_idx].bitmap, 18, 32, 0);
				display.refresh();
			}
		} else if (cmd == Command_t::LEFT_LONG) {
		}
	}
}

timeInterval_t DisplayUpdate()
{
	uint32_t start = HAL_GetTick();

	if (Speed < 20) {
		Speed = 0;
		Cadence = 0;
	}
	if (Cadence < 20)
		Cadence = 0;
	if (systemState != State_t::STOPPED)
		Duration += 1000;

	display.clearDisplayBuffer();
	if (currentDisplay == Display_t::RIDE) {
		drawSpeed(0, 0);
		drawCadence(128-54, 0);
		drawTime(0, 128-1*32);
		drawTemperature(0, 128-2*32);
		drawDuration(0, 32);
	} else {
		drawDistance(0, 0);
		drawMaximumSpeed(0, 32);
		drawDuration(0, 32*2);
		drawOdometer(0, 32*3);
	}
	display.refresh();

	if (Speed > 0) {
		// calculate speed for next update, if no sensor update occurs
		RTC_TimeTypeDef stime;
		RTC_DateTypeDef sdate;
		HAL_RTC_GetTime(&hrtc, &stime, RTC_FORMAT_BIN);
		HAL_RTC_GetDate(&hrtc, &sdate, RTC_FORMAT_BIN);
		uint32_t now = stime.Hours * 1000 * 60 * 60;
		now += stime.Minutes * 1000 * 60;
		now += stime.Seconds * 1000;
		now += 1000 * (stime.SecondFraction - stime.SubSeconds) / (stime.SecondFraction + 1);
		Speed = ((uint32_t)(wheelDiameter*3.6*10))/(now - SpdLastTime);
	} else {
		systemState = State_t::STOPPED;
		if (HAL_FLASHEx_DATAEEPROM_Unlock() == HAL_OK) {
			HAL_FLASHEx_DATAEEPROM_Program(FLASH_TYPEPROGRAMDATA_WORD, ODOMETER_ADR, Odometer);
			HAL_FLASHEx_DATAEEPROM_Lock();
		}

		return NEVER;
	}

	if (Cadence > 0) {
		// calculate cadence for next update, if no sensor update occurs
		RTC_TimeTypeDef stime;
		RTC_DateTypeDef sdate;
		HAL_RTC_GetTime(&hrtc, &stime, RTC_FORMAT_BIN);
		HAL_RTC_GetDate(&hrtc, &sdate, RTC_FORMAT_BIN);
		uint32_t now = stime.Hours * 1000 * 60 * 60;
		now += stime.Minutes * 1000 * 60;
		now += stime.Seconds * 1000;
		now += 1000 * (stime.SecondFraction - stime.SubSeconds) / (stime.SecondFraction + 1);
		Cadence = 60000 / (sensorQ[sensorH].Time - CadLastTime);
	}

	return 1000 - (HAL_GetTick() - start);
}

timeInterval_t ControlUpdateLeft()
{
	timeInterval_t result = NEVER;
	if ((leftLastState == GPIO_PIN_SET) && (HAL_GPIO_ReadPin(GPIOA, LEFT_Pin) == GPIO_PIN_RESET)) {
		// left button has closed
		leftStartTime = HAL_GetTick();
		leftLastState = GPIO_PIN_RESET;
		result = 20;
	} else if ((leftLastState == GPIO_PIN_RESET) && (HAL_GPIO_ReadPin(GPIOA, LEFT_Pin) == GPIO_PIN_RESET)) {
		// left button still closed
		result = 20;
	} else if ((leftLastState == GPIO_PIN_RESET) && (HAL_GPIO_ReadPin(GPIOA, LEFT_Pin) == GPIO_PIN_SET)) {
		// left button has opened
		uint32_t dur = HAL_GetTick() - leftStartTime;
		leftStartTime = HAL_GetTick();
		if ((dur >= SHORT_MIN) && (dur <= SHORT_MAX))
			CommandProcess(Command_t::LEFT_SHORT);
		if ((dur >= LONG_MIN) && (dur <= LONG_MAX))
			CommandProcess(Command_t::LEFT_LONG);
		leftLastState = GPIO_PIN_SET;
		result = 20;
	} // else left button still opened => done (return NEVER)
	return result;
}

timeInterval_t ControlUpdateRight()
{
	timeInterval_t result = NEVER;
	if ((rightLastState == GPIO_PIN_SET) && (HAL_GPIO_ReadPin(GPIOA, RIGHT_Pin) == GPIO_PIN_RESET)) {
		// right button has closed
		rightStartTime = HAL_GetTick();
		rightLastState = GPIO_PIN_RESET;
		result = 20;
	} else if ((rightLastState == GPIO_PIN_RESET) && (HAL_GPIO_ReadPin(GPIOA, RIGHT_Pin) == GPIO_PIN_RESET)) {
		// right button still closed
		result = 20;
	} else if ((rightLastState == GPIO_PIN_RESET) && (HAL_GPIO_ReadPin(GPIOA, RIGHT_Pin) == GPIO_PIN_SET)) {
		// right button has opened
		uint32_t dur = HAL_GetTick() - rightStartTime;
		rightStartTime = HAL_GetTick();
		if ((dur >= SHORT_MIN) && (dur <= SHORT_MAX))
			CommandProcess(Command_t::RIGHT_SHORT);
		if ((dur >= LONG_MIN) && (dur <= LONG_MAX))
			CommandProcess(Command_t::RIGHT_LONG);
		rightLastState = GPIO_PIN_SET;
		result = 20;
	} // else right button still opened => done (return NEVER)
	return result;
}

timeInterval_t SensorUpdate()
{
	while (sensorH != sensorT) {
		if (sensorQ[sensorH].Pin == WHEEL_Pin) {
			if (systemState == State_t::RUNNING) {
				// max speed: 7.65/60kph => 0.1275s
				if ((sensorQ[sensorH].Time - SpdLastTime) > 127) {
					Speed = ((uint32_t)(wheelDiameter*3.6*10))/(sensorQ[sensorH].Time - SpdLastTime);
					if (Speed > MaximumSpeed)
						MaximumSpeed = Speed;
					Distance++;
					Odometer++;
					SpdLastTime = sensorQ[sensorH].Time;
				}
			} else if (systemState == State_t::STOPPED) {
				Speed = 1;
				SpdLastTime = sensorQ[sensorH].Time;
				systemState = State_t::RUNNING;
				monitor.schedule((char*)"DisplayUpdate", 500);
			}
			// else if CONTROL then do nothing
		}
		// max rpm: 60/300rpm => 0.2s
		if ((sensorQ[sensorH].Pin == PEDAL_Pin) && ((sensorQ[sensorH].Time - CadLastTime) > 200)) {
			Cadence = 60000 / (sensorQ[sensorH].Time - CadLastTime);
			CadLastTime = sensorQ[sensorH].Time;
		}
		sensorH = (sensorH + 1) & 0xF;
	}
	return NEVER;
}


task_t TasksTable[] = {
	{ (char *)"DisplayUpdate", DisplayUpdate, NEVER, false },
	{ (char *)"ControlUpdateLeft", ControlUpdateLeft, NEVER, false },
	{ (char *)"ControlUpdateRight", ControlUpdateRight, NEVER, false },
	{ (char *)"SensorUpdate", SensorUpdate, NEVER, false },
};

/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow :
  *            System Clock source            = MSI
  *            MSI Range                      = 0
  *            SYSCLK(Hz)                     = 64000
  *            HCLK(Hz)                       = 32000
  *            AHB Prescaler                  = 2
  *            APB1 Prescaler                 = 1
  *            APB2 Prescaler                 = 1
  *            Main regulator output voltage  = Scale2 mode
  * @param  None
  * @retval None
  */
void SystemClock_Decrease(void)
{
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	/* Enable Power Control clock */
	__HAL_RCC_PWR_CLK_ENABLE();

	/* The voltage scaling allows optimizing the power consumption when the device is
	 clocked below the maximum system frequency, to update the voltage scaling value
	 regarding system frequency refer to product datasheet.  */
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

	/* Enable MSI Oscillator */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
	RCC_OscInitStruct.MSIState = RCC_MSI_ON;
	RCC_OscInitStruct.MSIClockRange       = RCC_MSIRANGE_5; /* Set temporary MSI range */
	RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct)!= HAL_OK)
		Error_Handler();

	/* Select MSI as system clock source and configure the HCLK, PCLK1 and PCLK2
	 clocks dividers */
	RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV2;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0)!= HAL_OK)
		Error_Handler();

	/* Set Final MSI range */
	/* Set MSI range to 0 */
	__HAL_RCC_MSI_RANGE_CONFIG(RCC_MSIRANGE_0);

	/* Configure the SysTick to have interrupt in 10 ms time basis*/
	HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/100);
}

/**
  * @brief  The application entry point.
  * @retval int
  */
int alt_main(void)
{
	// get wheel diameter from eeprom
	bool eepromValid = (readFromEEPROMu32(CHECK_ADR) == CHECK);
	if (eepromValid) {
		wheelDiameter = readFromEEPROMi16(WHEEL_ADR);
		Odometer = readFromEEPROMu32(ODOMETER_ADR);
	} else {
		if (HAL_FLASHEx_DATAEEPROM_Unlock() == HAL_OK) {
			HAL_FLASHEx_DATAEEPROM_Program(FLASH_TYPEPROGRAMDATA_WORD, CHECK_ADR, CHECK);
			HAL_FLASHEx_DATAEEPROM_Program(FLASH_TYPEPROGRAMDATA_WORD, ODOMETER_ADR, Odometer);
			HAL_FLASHEx_DATAEEPROM_Program(FLASH_TYPEPROGRAMDATA_HALFWORD, WHEEL_ADR, wheelDiameter);
			HAL_FLASHEx_DATAEEPROM_Lock();
		}

	}

	monitor.init(TasksTable, sizeof(TasksTable)/sizeof(task_t));

	display.clearDisplay();
#if 0
	display.drawBitmap(0, 0, epd_bitmap_splash_128x128, 128, 128, 0);
#else
	display.drawString(1, 48, bitmaps32, (char*)"23");
	display.drawString(46, 48, bitmaps32, (char*)"08");
	display.drawString(91, 48, bitmaps32, (char*)"08");
#endif
	display.refresh();

	/* Infinite loop */
	while (1)
	{
		monitor.run();
#if USE_SLEEP
	    /* Reduce the System clock to below 2 MHz */
	    SystemClock_Decrease();

		HAL_SuspendTick();
		HAL_SPI_MspDeInit(&hspi1);
		HAL_I2C_MspDeInit(&hi2c1);

		HAL_PWR_EnterSLEEPMode(PWR_LOWPOWERREGULATOR_ON, PWR_SLEEPENTRY_WFI);

	    SystemClock_Config();
		HAL_I2C_MspInit(&hi2c1);
		HAL_SPI_MspInit(&hspi1);
		HAL_ResumeTick();
#endif
	}
}

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc)
{
	if (systemState != State_t::SETUP)
		monitor.scheduleIrq((char*)"DisplayUpdate");
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	RTC_TimeTypeDef stime;
	RTC_DateTypeDef sdate;
	HAL_RTC_GetTime(&hrtc, &stime, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc, &sdate, RTC_FORMAT_BIN);

	if (GPIO_Pin == LEFT_Pin) {
		monitor.scheduleIrq((char*)"ControlUpdateLeft", 20);
	} else if (GPIO_Pin == RIGHT_Pin) {
		monitor.scheduleIrq((char*)"ControlUpdateRight", 20);
#if 0
		controlQ[controlT].Pin = GPIO_Pin;
		controlQ[controlT].Value = HAL_GPIO_ReadPin(GPIOA, GPIO_Pin);
		controlQ[controlT].Time = stime.Hours * 1000 * 60 * 60;
		controlQ[controlT].Time += stime.Minutes * 1000 * 60;
		controlQ[controlT].Time += stime.Seconds * 1000;
		controlQ[controlT].Time += 1000 * (stime.SecondFraction - stime.SubSeconds) / (stime.SecondFraction + 1);
		// De-bounce ??
		if (controlQ[controlT].Time != lastCqTime) {
			controlT = (controlT + 1) & 0xF;
			lastCqTime = controlQ[controlT].Time;
			monitor.scheduleIrq((char*)"ControlUpdate");
		}
#endif
	} else {  // WHEEL or PEDAL
		sensorQ[sensorT].Pin = GPIO_Pin;
		sensorQ[sensorT].Value = HAL_GPIO_ReadPin(GPIOA, GPIO_Pin);
		sensorQ[sensorT].Time = stime.Hours * 1000 * 60 * 60;
		sensorQ[sensorT].Time += stime.Minutes * 1000 * 60;
		sensorQ[sensorT].Time += stime.Seconds * 1000;
		sensorQ[sensorT].Time += 1000 * (stime.SecondFraction - stime.SubSeconds) / (stime.SecondFraction + 1);
		sensorT = (sensorT + 1) & 0xF;
		monitor.scheduleIrq((char*)"SensorUpdate");
	}
}
