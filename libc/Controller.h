/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 07.008.001
* Copyright (C) 2015 Sony Interactive Entertainment Inc.
*/

#ifndef _CONTROLLER_H_
#define _CONTROLLER_H_

#include <string>
#include <user_service.h>

#define MAX_PAD_NUM 4

namespace Controller
{
	namespace Input
	{
		typedef enum Button
		{
			BUTTON_SELECT = (1 << 0),
			BUTTON_L3 = (1 << 1),
			BUTTON_R3 = (1 << 2),
			BUTTON_START = (1 << 3),
			BUTTON_UP = (1 << 4),
			BUTTON_RIGHT = (1 << 5),
			BUTTON_DOWN = (1 << 6),
			BUTTON_LEFT = (1 << 7),
			BUTTON_L2 = (1 << 8),
			BUTTON_R2 = (1 << 9),
			BUTTON_L1 = (1 << 10),
			BUTTON_R1 = (1 << 11),
			BUTTON_TRIANGLE = (1 << 12),
			BUTTON_CIRCLE = (1 << 13),
			BUTTON_CROSS = (1 << 14),
			BUTTON_SQUARE = (1 << 15)
		} Button;

		typedef enum ButtonEventPattern
		{
			PATTERN_ANY,
			PATTERN_ALL,
		} ButtonEventPattern;

		class ControllerContext {
			double m_previousSeconds;
			double m_initialSecondsUntilNextRepeat;
			double m_finalSecondsUntilNextRepeat;
			double m_secondsUntilNextRepeat;
		public:

			ControllerContext(void);
			~ControllerContext(void);

			int initialize(SceUserServiceUserId userId);
			int finalize(void);
			void update(double seconds);

			// check whether any or all of the specified button are down
			bool isButtonDown(uint32_t port, uint32_t buttons, ButtonEventPattern pattern = PATTERN_ALL) const;
			bool isButtonUp(uint32_t port, uint32_t buttons, ButtonEventPattern pattern = PATTERN_ALL) const;
			bool isButtonPressed(uint32_t port, uint32_t buttons, ButtonEventPattern pattern = PATTERN_ALL) const;
			bool isButtonReleased(uint32_t port, uint32_t buttons, ButtonEventPattern pattern = PATTERN_ALL) const;

			//const Vector2& getLeftStick(uint32_t port) const;
			//const Vector2& getRightStick(uint32_t port) const;

			void setDeadZone(float deadZone);

		private:
			typedef struct AnalogStick
			{
				uint8_t x;
				uint8_t y;
			} AnalogStick;

			typedef struct AnalogButtons
			{
				uint8_t l2;
				uint8_t r2;
				uint8_t padding[2]; /*padding*/
			} AnalogButtons;

			typedef struct Data
			{
				uint32_t		buttons;
				AnalogStick		leftStick;
				AnalogStick		rightStick;
				AnalogButtons	analogButtons;
				bool			connected;
			} Data;

			Data m_currentPadData[MAX_PAD_NUM];
			Data m_temporaryPadData;
			//Vector2	m_leftStickXY[MAX_PAD_NUM];
			//Vector2	m_rightStickXY[MAX_PAD_NUM];
			//Vector2	m_dummyStickXY;

			uint32_t		m_pressedButtonData[MAX_PAD_NUM];		///< The "Pressed" button event data.
			uint32_t		m_releasedButtonData[MAX_PAD_NUM];		///< The "Released" button event data.

			float					m_deadZone;
			static const float		m_defaultDeadZone;
			static const float		m_recipMaxByteAsFloat;
			int32_t					m_handle;
			void updatePadData(void);
		};
	}
}

#endif /* _FRAMEWORK_CONTROLLER_H */
