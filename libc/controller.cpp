/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 07.008.001
* Copyright (C) 2015 Sony Interactive Entertainment Inc.
*/

#include "controller.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <string.h>
#include <sceerror.h>
#include <sys/event.h>
#include <unistd.h>
#include <pad.h>

const float Controller::Input::ControllerContext::m_defaultDeadZone = 0.25;
const float Controller::Input::ControllerContext::m_recipMaxByteAsFloat = 1.0f / 255.0f;

Controller::Input::ControllerContext::ControllerContext(void)
{

}

Controller::Input::ControllerContext::~ControllerContext(void)
{

}

int Controller::Input::ControllerContext::initialize(SceUserServiceUserId userId)
{
	m_previousSeconds = 0;
	m_initialSecondsUntilNextRepeat = 0.25;
	m_finalSecondsUntilNextRepeat = 0.05;
	m_secondsUntilNextRepeat = m_initialSecondsUntilNextRepeat;

	m_deadZone = m_defaultDeadZone;

	int ret = scePadInit();
	printf("gsc: scePadInit() Ret\n");
	if (ret != SCE_OK)
	{
		printf("scePadInit:: Controller Initialization Failed : 0x%08X\n", ret);
		return ret;
	}

	if (userId < 0)
	{
		SceUserServiceInitializeParams param;
		param.priority = SCE_KERNEL_PRIO_FIFO_LOWEST;
		sceUserServiceInitialize(&param);
		sceUserServiceGetInitialUser(&userId);
	}
	printf("gsc: userId\n");

	m_handle = scePadOpen(userId, SCE_PAD_PORT_TYPE_STANDARD, 0, NULL);
	if (m_handle < 0)
	{
		printf("scePadOpen:: Controller Port Opening Failed : 0x%08X\n", m_handle);
		return m_handle;
	}
	printf("gsc: scePadOpen() \n");

	memset(&m_temporaryPadData, 0, sizeof(m_temporaryPadData));
	m_temporaryPadData.leftStick.x = 128;
	m_temporaryPadData.leftStick.y = 128;
	m_temporaryPadData.rightStick.x = 128;
	m_temporaryPadData.rightStick.y = 128;

	memset(m_currentPadData, 0x0, sizeof(Data) * MAX_PAD_NUM);
	for (int i = 0; i < MAX_PAD_NUM; ++i)
	{
		m_currentPadData[i].leftStick.x = 128;
		m_currentPadData[i].leftStick.y = 128;
		m_currentPadData[i].rightStick.x = 128;
		m_currentPadData[i].rightStick.y = 128;
	}
	/*
	for (int i = 0; i < MAX_PAD_NUM; i++) {
		m_leftStickXY[i].setX(0.0f);
		m_leftStickXY[i].setY(0.0f);
		m_rightStickXY[i].setX(0.0f);
		m_rightStickXY[i].setY(0.0f);
	}
	m_dummyStickXY.setX(0.0f);
	m_dummyStickXY.setY(0.0f);
	*/

	return SCE_OK;
}

int Controller::Input::ControllerContext::finalize()
{
	return SCE_OK;
}

void Controller::Input::ControllerContext::update(double seconds)
{
	Data previousPadData[MAX_PAD_NUM];

	memcpy(previousPadData, m_currentPadData, sizeof(previousPadData));

	if (seconds - m_previousSeconds > m_secondsUntilNextRepeat)
	{
		memset(previousPadData, 0, sizeof(previousPadData));
		m_previousSeconds = seconds;
		m_secondsUntilNextRepeat = m_secondsUntilNextRepeat * 0.75 + m_finalSecondsUntilNextRepeat * 0.25;
	}

	memset(m_currentPadData, 0x0, sizeof(Data) * MAX_PAD_NUM);
	updatePadData();

	if (m_currentPadData[0].buttons == 0)
	{
		m_previousSeconds = seconds;
		m_secondsUntilNextRepeat = m_initialSecondsUntilNextRepeat;
	}

	float lX, lY, rX, rY;

	for (int i = 0; i < MAX_PAD_NUM; i++) {
		// ascertain button pressed / released events from the current & previous state
		m_pressedButtonData[i] = (m_currentPadData[i].buttons & ~previousPadData[i].buttons);		///< pressed button event data
		m_releasedButtonData[i] = (~m_currentPadData[i].buttons & previousPadData[i].buttons);		///< released button event data

		// Get the analogue stick values
		// Remap ranges from 0-255 to -1 > +1
		lX = (float)((int32_t)m_currentPadData[i].leftStick.x * 2 - 256) * m_recipMaxByteAsFloat;
		lY = (float)((int32_t)m_currentPadData[i].leftStick.y * 2 - 256) * m_recipMaxByteAsFloat;
		rX = (float)((int32_t)m_currentPadData[i].rightStick.x * 2 - 256) * m_recipMaxByteAsFloat;
		rY = (float)((int32_t)m_currentPadData[i].rightStick.y * 2 - 256) * m_recipMaxByteAsFloat;

		// store stick values adjusting for deadzone
		/*
		m_leftStickXY[i].setX((fabsf(lX) < m_deadZone) ? 0.0f : lX);
		m_leftStickXY[i].setY((fabsf(lY) < m_deadZone) ? 0.0f : lY);
		m_rightStickXY[i].setX((fabsf(rX) < m_deadZone) ? 0.0f : rX);
		m_rightStickXY[i].setY((fabsf(rY) < m_deadZone) ? 0.0f : rY);
		*/
	}

}

bool Controller::Input::ControllerContext::isButtonDown(uint32_t port, uint32_t buttons, ButtonEventPattern pattern) const
{
	if (port >= MAX_PAD_NUM) {
		return false;
	}

	if (pattern == PATTERN_ANY)
	{
		if ((m_currentPadData[port].buttons & buttons) != 0)
		{
			return true;
		}
		else {
			return false;
		}
	}
	else if (pattern == PATTERN_ALL) {
		if ((m_currentPadData[port].buttons & buttons) == buttons)
		{
			return true;
		}
		else {
			return false;
		}
	}
	return false;
}


bool Controller::Input::ControllerContext::isButtonUp(uint32_t port, uint32_t buttons, ButtonEventPattern pattern) const
{
	if (port >= MAX_PAD_NUM) {
		return true;
	}

	if (pattern == PATTERN_ANY)
	{
		if ((m_currentPadData[port].buttons & buttons) != 0)
		{
			return false;
		}
		else {
			return true;
		}
	}
	else if (pattern == PATTERN_ALL) {
		if ((m_currentPadData[port].buttons & buttons) == buttons)
		{
			return false;
		}
		else {
			return true;
		}
	}
	return true;
}

bool Controller::Input::ControllerContext::isButtonPressed(uint32_t port, uint32_t buttons, ButtonEventPattern pattern) const
{
	if (port >= MAX_PAD_NUM) {
		return false;
	}

	if (pattern == PATTERN_ANY)
	{
		if ((m_pressedButtonData[port] & buttons) != 0)
		{
			return true;
		}
		else {
			return false;
		}
	}
	else if (pattern == PATTERN_ALL) {
		if ((m_pressedButtonData[port] & buttons) == buttons)
		{
			return true;
		}
		else {
			return false;
		}
	}
	return false;
}

bool Controller::Input::ControllerContext::isButtonReleased(uint32_t port, uint32_t buttons, ButtonEventPattern pattern) const
{
	if (port >= MAX_PAD_NUM) {
		return false;
	}

	if (pattern == PATTERN_ANY)
	{
		if ((m_releasedButtonData[port] & buttons) != 0)
		{
			return true;
		}
		else {
			return false;
		}
	}
	else if (pattern == PATTERN_ALL) {
		if ((m_releasedButtonData[port] & buttons) == buttons)
		{
			return true;
		}
		else {
			return false;
		}
	}
	return false;
}
/*
const Vector2& Controller::Input::ControllerContext::getLeftStick(uint32_t port) const
{
	if (port >= MAX_PAD_NUM) {
		return m_dummyStickXY;
	}

	return m_leftStickXY[port];
}

const Vector2& Controller::Input::ControllerContext::getRightStick(uint32_t port) const
{
	if (port >= MAX_PAD_NUM) {
		return m_dummyStickXY;
	}

	return m_rightStickXY[port];
}
*/
void Controller::Input::ControllerContext::setDeadZone(float deadZone)
{
	m_deadZone = deadZone;
}


void Controller::Input::ControllerContext::updatePadData(void)
{
	for (int i = 0; i < MAX_PAD_NUM; i++) {
		ScePadData data;
		int ret = scePadReadState(m_handle, &data);
		if (ret == SCE_OK) {
			m_currentPadData[i].buttons = data.buttons;
			m_currentPadData[i].leftStick.x = data.leftStick.x;
			m_currentPadData[i].leftStick.y = data.leftStick.y;
			m_currentPadData[i].rightStick.x = data.rightStick.x;
			m_currentPadData[i].rightStick.y = data.rightStick.y;
			m_currentPadData[i].analogButtons.l2 = data.analogButtons.l2;
			m_currentPadData[i].analogButtons.r2 = data.analogButtons.r2;
			m_currentPadData[i].connected = data.connected;
		}
	}
}

