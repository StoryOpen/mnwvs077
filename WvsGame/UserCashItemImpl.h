/*
09/12/2019 added.
Implementations of cash item usages.
*/

#pragma once

class User;
class InPacket;
class OutPacket;

class UserCashItemImpl
{
public:

	static int ConsumeSpeakerChannel(User *pUser, InPacket *iPacket);
	static int ConsumeSpeakerWorld(User *pUser, int nCashItemType, InPacket *iPacket);
	static int ConsumeAvatarMegaphone(User *pUser, int nItemID, InPacket *iPacket);
};

