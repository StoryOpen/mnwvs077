#include "LifePool.h"
#include "..\WvsLib\Net\InPacket.h"
#include "..\WvsLib\Net\OutPacket.h"
#include "..\WvsLib\Net\PacketFlags\MobPacketFlags.hpp"
#include "..\WvsLib\Net\PacketFlags\UserPacketFlags.hpp"

#include "..\WvsLib\Logger\WvsLogger.h"

#include "User.h"
#include "MobTemplate.h"
#include "Field.h"
#include "Controller.h"
#include "SkillEntry.h"
#include "Drop.h"
#include "AttackInfo.h"
#include "SecondaryStat.h"
#include "NpcTemplate.h"
#include "..\WvsLib\Common\WvsGameConstants.hpp"

#include <cmath>


LifePool::LifePool()
	: m_pCtrlNull(AllocObjCtor(Controller)(nullptr))
{
}


LifePool::~LifePool()
{
	for (auto& p : m_mMob)
		FreeObj( p.second );
	for (auto& p : m_aNpcGen)
		FreeObj( p.second );
	for (auto& p : m_hCtrl)
		FreeObj( p.second );
	FreeObj( m_pCtrlNull );
}

void LifePool::Init(Field* pField, int nFieldID)
{
	m_pField = pField;

	int nSizeX = 1920;
	int nSizeY = 1080; //I dont know
	int nGenSize = (int)(((double)nSizeX * nSizeY) * 0.0000048125f);
	if (nGenSize < 1)
		nGenSize = 1;
	else if (nGenSize >= MAX_MOB_GEN)
		nGenSize = MAX_MOB_GEN;
	m_nMobCapacityMin = nGenSize;
	m_nMobCapacityMax = nGenSize * 2 * pField->GetMobRate();

	auto& mapWz = stWzResMan->GetWz(Wz::Map)["Map"]
		["Map" + std::to_string(nFieldID / 100000000)]
		[StringUtility::LeftPadding(std::to_string(nFieldID), 9, '0')];

	auto& lifeData = mapWz["life"];
	for (auto& node : lifeData)
	{
		const auto &typeFlag = (std::string)node["type"];
		if (typeFlag == "n")
			LoadNpcData(node);
		else if (typeFlag == "m")
			LoadMobData(node);
	}	

	//�j��ͦ��Ҧ�NPC
	for (auto& npc : m_lNpc)
		CreateNpc(npc);

	TryCreateMob(false);
	//mapWz.ReleaseData();
}

void LifePool::SetFieldObjAttribute(FieldObj* pFieldObj, WZ::Node& dataNode)
{
	try {
		pFieldObj->SetPosX(dataNode["x"]);
		pFieldObj->SetPosY(dataNode["y"]);
		pFieldObj->SetF(dataNode["f"]);
		pFieldObj->SetHide(dataNode["hide"]);
		pFieldObj->SetFh(dataNode["fh"]);
		pFieldObj->SetCy(dataNode["cy"]);
		pFieldObj->SetRx0(dataNode["rx0"]);
		pFieldObj->SetRx1(dataNode["rx1"]);
		pFieldObj->SetTemplateID(atoi(((std::string)dataNode["id"]).c_str()));
	}
	catch (std::exception& e) {
		WvsLogger::LogFormat(WvsLogger::LEVEL_ERROR, "Ū���a�Ϫ���o�Ϳ��~�A�T��:%s\n", e.what());
	}
}

void LifePool::SetMaxMobCapacity(int max)
{
	m_nMobCapacityMax = max;
}

int LifePool::GetMaxMobCapacity() const
{
	return m_nMobCapacityMax;
}

void LifePool::LoadNpcData(WZ::Node& dataNode)
{
	Npc npc;
	SetFieldObjAttribute(&npc, dataNode);
	npc.SetTemplate(NpcTemplate::GetInstance()->GetNpcTemplate(npc.GetTemplateID()));
	m_lNpc.push_back(npc);
}

void LifePool::LoadMobData(WZ::Node& dataNode)
{
	Mob mob;
	SetFieldObjAttribute(&mob, dataNode);
	mob.SetMobTemplate(MobTemplate::GetMobTemplate(mob.GetTemplateID()));
	//MobTemplate::GetMobTemplate(mob.GetTemplateID());
	m_aMobGen.push_back(mob);
}

void LifePool::CreateNpc(const Npc& npc)
{
	std::lock_guard<std::mutex> lock(m_lifePoolMutex);
	Npc* newNpc = AllocObj(Npc);
	*newNpc = npc; //Should notice pointer data assignment
	newNpc->SetFieldObjectID(atomicObjectCounter++);
	newNpc->SetField(m_pField);
	m_aNpcGen.insert({ newNpc->GetFieldObjectID(), newNpc });
}

void LifePool::TryCreateMob(bool reset)
{
	std::lock_guard<std::mutex> lock(m_lifePoolMutex);
	/*
	if reset, kill all monsters and respawns
	*/
	if(m_aMobGen.size() > 0)
		for (int i = 0; i < m_nMobCapacityMax - ((int)m_mMob.size()); ++i) 
		{
			auto& mob = m_aMobGen[rand() % m_aMobGen.size()];
			CreateMob(mob, mob.GetPosX(), mob.GetPosY(), mob.GetFh(), 0, -2, 0, 0, 0, nullptr);
		}
}

void LifePool::CreateMob(const Mob& mob, int x, int y, int fh, int bNoDropPriority, int nType, unsigned int dwOption, int bLeft, int nMobType, Controller* pOwner)
{
	Controller* pController = pOwner;
	if (m_hCtrl.size() > 0)
		pController = m_hCtrl.begin()->second;

	if (pController != nullptr 
		&& (pController->GetMobCtrlCount() + pController->GetNpcCtrlCount() - (pController->GetMobCtrlCount() != 0) >= 50)
		&& (nType != -3 || nMobType != 2 || !GiveUpMobController(pController)))
		pController = nullptr;

	if (pController && pController != this->m_pCtrlNull)
	{
		Mob* newMob = AllocObj( Mob );
		*newMob = mob;
		newMob->SetField(m_pField);
		newMob->SetFieldObjectID(atomicObjectCounter++);

		int moveAbility = newMob->GetMobTemplate()->m_nMoveAbility;

		newMob->SetHP(newMob->GetMobTemplate()->m_lnMaxHP);
		newMob->SetMP((int)newMob->GetMobTemplate()->m_lnMaxMP);
		newMob->SetMovePosition(x, y, bLeft & 1 | 2 * (moveAbility == 3 ? 6 : (moveAbility == 0 ? 1 : 0) + 1), fh);
		newMob->SetMoveAction(5); //�Ǫ� = 5 ?

		OutPacket createMobPacket;
		newMob->MakeEnterFieldPacket(&createMobPacket);
		m_pField->BroadcastPacket(&createMobPacket);

		newMob->SetController(pController);
		newMob->SendChangeControllerPacket(pController->GetUser(), 1);
		pController->AddCtrlMob(newMob);

		//19/05/07 +
		UpdateCtrlHeap(pController);
		m_mMob.insert({ newMob->GetFieldObjectID(), newMob });
	}
}

void LifePool::RemoveMob(Mob * pMob)
{
	if (pMob == nullptr)
		return;
	auto pController = pMob->GetController();
	if (pController->GetUser() != nullptr)
	{
		pController->RemoveCtrlMob(pMob);
		pMob->SendReleaseControllPacket(pController->GetUser(), pMob->GetFieldObjectID());
	}
	else
		m_pCtrlNull->RemoveCtrlMob(pMob);
	OutPacket oPacket;
	pMob->MakeLeaveFieldPacket(&oPacket);
	m_pField->SplitSendPacket(&oPacket, nullptr);
	m_mMob.erase(pMob->GetFieldObjectID());
	FreeObj( pMob );
}

void LifePool::OnEnter(User *pUser)
{
	std::lock_guard<std::mutex> lock(m_lifePoolMutex);
	InsertController(pUser);

	for (auto& npc : m_aNpcGen)
	{
		OutPacket oPacket;
		npc.second->MakeEnterFieldPacket(&oPacket);
		npc.second->SendChangeControllerPacket(pUser);
		pUser->SendPacket(&oPacket);
	}
	//WvsLogger::LogFormat("LifePool::OnEnter : Total Mob = %d\n", m_aMobGen.size());
	for (auto& mob : m_mMob)
	{
		OutPacket oPacket;
		mob.second->MakeEnterFieldPacket(&oPacket);
		pUser->SendPacket(&oPacket);
	}
	//WvsLogger::LogFormat("LifePool::OnEnter : Total Controlled = %d Null Controlled = %d\n size of m_hCtrl = %d", m_mController[pUser->GetUserID()]->second->GetTotalControlledCount(), m_pCtrlNull->GetTotalControlledCount(), m_hCtrl.size());
}

void LifePool::InsertController(User* pUser)
{
	Controller* controller = AllocObjCtor(Controller)(pUser) ;
	auto& iter = m_hCtrl.insert({ 0, controller });
	m_mController.insert({ pUser->GetUserID(), iter });
	RedistributeLife();
}

void LifePool::RemoveController(User* pUser)
{
	std::lock_guard<std::mutex> lock(m_lifePoolMutex);
	if (m_mController.size() == 0)
		return;

	//���pUser������iterator
	auto& iter = m_mController.find(pUser->GetUserID());

	//�ھ�iterator���controller����
	auto pController = iter->second->second;

	//�qhCtrl��������controller
	m_hCtrl.erase(iter->second);

	//�qpUser������iter
	m_mController.erase(iter);

	auto& controlled = pController->GetMobCtrlList();
	for (auto pMob : controlled)
	{
		Controller* pCtrlNew = m_pCtrlNull;
		if (m_hCtrl.size() > 0)
			pCtrlNew = m_hCtrl.begin()->second;
		pMob->SendChangeControllerPacket(pController->GetUser(), 0);
		pMob->SetController(pCtrlNew);
		pCtrlNew->AddCtrlMob(pMob);
		if (pCtrlNew != m_pCtrlNull)
		{
			pMob->SendChangeControllerPacket(pCtrlNew->GetUser(), 1);
			UpdateCtrlHeap(pCtrlNew);
		}
	}

	//�P��
	FreeObj( pController );
}

void LifePool::UpdateCtrlHeap(Controller * pController)
{
	//�ھ�controller��������pUser
	auto pUser = pController->GetUser();

	//���pUser������iterator
	auto& iter = m_mController.find(pUser->GetUserID());


	//�qhCtrl��������controller�A�í��s���J [�s���ƶq��key]
	m_hCtrl.erase(iter->second);
	m_mController[pUser->GetUserID()] = m_hCtrl.insert({ pController->GetTotalControlledCount(), pController });
}

bool LifePool::GiveUpMobController(Controller * pController)
{
	return false;
}

void LifePool::RedistributeLife()
{
	Controller* pCtrl = nullptr;
	int nCtrlCount = (int)m_hCtrl.size();
	if (nCtrlCount > 0)
	{
		auto& nonControlled = m_pCtrlNull->GetMobCtrlList();
		//for (auto pMob : nonControlled)
		for(auto iter = nonControlled.begin(); iter != nonControlled.end(); )
		{
			auto pMob = *iter;
			pCtrl = m_hCtrl.begin()->second;

			//����NPC�P�Ǫ��ƶq�`�M�W�L50�A���s�t�m
			if (pCtrl->GetTotalControlledCount() >= 50)
				break;
			pCtrl->AddCtrlMob(pMob);
			pMob->SetController(pCtrl);
			pMob->SendChangeControllerPacket(pCtrl->GetUser(), 1);
			UpdateCtrlHeap(pCtrl);

			m_pCtrlNull->RemoveCtrlMob(pMob);
			iter = nonControlled.begin();
		}
		//NPC

		Controller* minCtrl, *maxCtrl;
		int nMaxNpcCtrl, nMaxMobCtrl, nMinNpcCtrl, nMinMobCtrl;
		//���s�հt�C�ӤH���Ǫ������v
		if (nCtrlCount >= 2) //�ܤ֤@��minCtrl�PmaxCtrl
		{
			while (true) 
			{
				minCtrl = m_hCtrl.begin()->second;
				maxCtrl = m_hCtrl.rbegin()->second;
				nMaxNpcCtrl = maxCtrl->GetNpcCtrlCount();
				nMaxMobCtrl = maxCtrl->GetMobCtrlCount();
				nMinNpcCtrl = minCtrl->GetNpcCtrlCount();
				nMinMobCtrl = minCtrl->GetMobCtrlCount();
				WvsLogger::LogFormat("Min Ctrl User = %d(%d), Max Ctrl User = %d(%d)\n", minCtrl->GetUser()->GetUserID(), nMinMobCtrl, maxCtrl->GetUser()->GetUserID(), nMaxMobCtrl);
				//�w�g�������Ť��ݭn�A���s�t��
				if ((nMaxNpcCtrl + nMaxMobCtrl - (nMaxMobCtrl != 0) <= (nMinNpcCtrl - (nMinMobCtrl != 0) + nMinMobCtrl + 1))
					|| ((nMaxNpcCtrl + nMaxMobCtrl - (nMaxMobCtrl != 0)) <= 10))
					break;
				WvsLogger::LogFormat("Unbalanced.\n", minCtrl->GetUser()->GetUserID(), nMinMobCtrl, maxCtrl->GetUser()->GetUserID(), nMaxMobCtrl);
				Mob* pMob = *(maxCtrl->GetMobCtrlList().rbegin());
				maxCtrl->GetMobCtrlList().pop_back();
				pMob->SendChangeControllerPacket(maxCtrl->GetUser(), 0);

				minCtrl->AddCtrlMob(pMob);
				pMob->SetController(minCtrl);
				pMob->SendChangeControllerPacket(minCtrl->GetUser(), 1);
				UpdateCtrlHeap(minCtrl);
				UpdateCtrlHeap(maxCtrl);
			}
		}
	}
}

void LifePool::Update()
{
	TryCreateMob(false);
}

void LifePool::OnPacket(User * pUser, int nType, InPacket * iPacket)
{
	if (nType >= FlagMin(MobRecvPacketFlag) && nType <= FlagMax(MobRecvPacketFlag))
	{
		OnMobPacket(pUser, nType, iPacket);
	}
	else if (nType == 0x384)
		OnNpcPacket(pUser, nType, iPacket);
}

void LifePool::OnUserAttack(User * pUser, const SkillEntry * pSkill, AttackInfo * pInfo)
{
	std::lock_guard<std::mutex> mobLock(this->m_lifePoolMutex);

	OutPacket attackPacket;
	EncodeAttackInfo(pUser, pInfo, &attackPacket);
	m_pField->SplitSendPacket(&attackPacket, nullptr);

	for (const auto& dmgInfo : pInfo->m_mDmgInfo)
	{
		auto mobIter = m_mMob.find(dmgInfo.first);
		if (mobIter == m_mMob.end())
			continue;
		auto pMob = mobIter->second;
		auto& refDmgValues = dmgInfo.second;
		for (const auto& dmgValue : refDmgValues)
		{
			//printf("Monster %d Damaged : %d Total : %d\n", dmgInfo.first, dmgValue, pMob->GetMobTemplate()->m_lnMaxHP);
			pMob->OnMobHit(pUser, dmgValue, pInfo->m_nType);
			if (pMob->GetHP() <= 0)
			{
				pMob->OnMobDead(
					pInfo->m_nX,
					pInfo->m_nY,
					pUser->GetSecondaryStat()->nMesoUp,
					pUser->GetSecondaryStat()->nMesoUpByItem
				);
				RemoveMob(pMob);
				break;
			}
		}
	}
}

void LifePool::EncodeAttackInfo(User * pUser, AttackInfo * pInfo, OutPacket * oPacket)
{
	oPacket->Encode2(pInfo->m_nType - UserRecvPacketFlag::User_OnUserAttack_MeleeAttack + UserSendPacketFlag::UserRemote_OnMeleeAttack);
	oPacket->Encode4(pUser->GetUserID());
	oPacket->Encode1(pInfo->m_bAttackInfoFlag);
	if (pInfo->m_nSkillID > 0 || pInfo->m_nType == UserRecvPacketFlag::User_OnUserAttack_MagicAttack)
	{
		oPacket->Encode1(pInfo->m_nSLV);
		oPacket->Encode4(pInfo->m_nSkillID);
	}
	else
		oPacket->Encode1(0);

	oPacket->Encode1(pInfo->m_nDisplay);
	oPacket->Encode1(pInfo->m_nAttackActionType);
	oPacket->Encode1(pInfo->m_nAttackSpeed);

	oPacket->Encode4(pInfo->m_nCsStar);
	for (const auto& dmgInfo : pInfo->m_mDmgInfo)
	{
		oPacket->Encode4(dmgInfo.first);
		oPacket->Encode1(7);
		if (pInfo->m_nSkillID == 4211006)
			oPacket->Encode1((char)dmgInfo.second.size());
		for (const auto& dmgValue : dmgInfo.second)
			oPacket->Encode4((int)dmgValue);
	}

	if (pInfo->m_nType == UserRecvPacketFlag::User_OnUserAttack_ShootAttack)
	{
		oPacket->Encode2(pUser->GetPosX());
		oPacket->Encode2(pUser->GetPosY());
	}
	else if (pInfo->m_nType == UserRecvPacketFlag::User_OnUserAttack_MagicAttack)
		oPacket->Encode4(pInfo->m_tKeyDown);
}

std::mutex & LifePool::GetLock()
{
	return m_lifePoolMutex;
}

Npc * LifePool::GetNpc(int nFieldObjID)
{
	auto findIter = m_aNpcGen.find(nFieldObjID);
	if (findIter != m_aNpcGen.end())
		return findIter->second;
	return nullptr;
}

Mob * LifePool::GetMob(int nFieldObjID)
{
	auto findIter = m_mMob.find(nFieldObjID);
	if (findIter != m_mMob.end())
		return findIter->second;
	return nullptr;
}

void LifePool::UpdateMobSplit(User * pUser)
{
}

void LifePool::OnMobPacket(User * pUser, int nType, InPacket * iPacket)
{
	int dwMobID = iPacket->Decode4();
	//std::lock_guard<std::mutex> lock(m_lifePoolMutex);

	auto mobIter = m_mMob.find(dwMobID);
	if (mobIter != m_mMob.end()) {
		switch (nType)
		{
		case MobRecvPacketFlag::Mob_OnMove:
			m_pField->OnMobMove(pUser, mobIter->second, iPacket);
			break;
		}
	}
	else {
		//Release Controller
	}
}

void LifePool::OnNpcPacket(User * pUser, int nType, InPacket * iPacket)
{
	std::lock_guard<std::mutex> lock(m_lifePoolMutex);
	if (nType == 0x384)
	{
		auto iterNpc = this->m_aNpcGen.find(iPacket->Decode4());
		if (iterNpc != m_aNpcGen.end())
		{
			iterNpc->second->OnUpdateLimitedInfo(pUser, iPacket);
		}
	}
}