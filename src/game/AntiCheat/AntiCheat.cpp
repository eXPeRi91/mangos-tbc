#include "AntiCheat.h"
#include "Entities/CPlayer.h"

AntiCheat::AntiCheat(CPlayer* player)
{
    m_Player = player;

    oldMoveInfo = MovementInfo();
    storedMoveInfo = MovementInfo();

    m_Initialized = false;
    m_CanFly = false;
    m_CanWaterwalk = false;

    player->AddAntiCheatModule(this);
}

bool AntiCheat::HandleMovement(MovementInfo& MoveInfo, Opcodes opcode, bool cheat)
{
    newMoveInfo = MoveInfo;
    newMapID = m_Player->GetMapId();

    if (m_Player->HasAuraType(SPELL_AURA_FLY) || m_Player->HasAuraType(SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED) || m_Player->GetGMFly())
        m_CanFly = true;
    else if (opcode == CMSG_MOVE_SET_CAN_FLY_ACK) // Trust that client will send ack when he's told not to fly anymore
        m_CanFly = false;

    if (m_Player->HasAuraType(SPELL_AURA_WATER_WALK) || m_Player->HasAuraType(SPELL_AURA_GHOST))
        m_CanWaterwalk = true;
    else if (opcode == CMSG_MOVE_WATER_WALK_ACK)
        m_CanWaterwalk = false;


    switch (opcode)
    {
    case CMSG_FORCE_WALK_SPEED_CHANGE_ACK:
        AllowedSpeed[MOVE_WALK] = m_Player->GetSpeed(MOVE_WALK);
        break;
    case CMSG_FORCE_RUN_SPEED_CHANGE_ACK:
        AllowedSpeed[MOVE_RUN] = m_Player->GetSpeed(MOVE_RUN);
        break;
    case CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK:
        AllowedSpeed[MOVE_RUN_BACK] = m_Player->GetSpeed(MOVE_RUN_BACK);
        break;
    case CMSG_FORCE_SWIM_SPEED_CHANGE_ACK:
        AllowedSpeed[MOVE_SWIM] = m_Player->GetSpeed(MOVE_SWIM);
        break;
    case CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK:
        AllowedSpeed[MOVE_SWIM_BACK] = m_Player->GetSpeed(MOVE_SWIM_BACK);
        break;
    case CMSG_FORCE_TURN_RATE_CHANGE_ACK:
        AllowedSpeed[MOVE_TURN_RATE] = m_Player->GetSpeed(MOVE_TURN_RATE);
        break;
    case CMSG_FORCE_FLIGHT_SPEED_CHANGE_ACK:
        AllowedSpeed[MOVE_FLIGHT] = m_Player->GetSpeed(MOVE_FLIGHT);
        break;
    case CMSG_FORCE_FLIGHT_BACK_SPEED_CHANGE_ACK:
        AllowedSpeed[MOVE_FLIGHT_BACK] = m_Player->GetSpeed(MOVE_FLIGHT_BACK);
        break;
    default: break;
    }

    for (uint8 i = 0; i < MAX_MOVE_TYPE; ++i)
        if (m_Player->GetSpeed(UnitMoveType(i)) > AllowedSpeed[UnitMoveType(i)])
            AllowedSpeed[UnitMoveType(i)] = m_Player->GetSpeed(UnitMoveType(i));

    return false;
}

void AntiCheat::HandleRelocate(float x, float y, float z, float o)
{
    if (m_Player->IsTaxiFlying())
        oldMoveInfo.ChangePosition(x, y, z, o);
}

void AntiCheat::HandleTeleport(uint32 map, float x, float y, float z, float o)
{
    oldMoveInfo.ChangePosition(x, y, z, o);
    oldMapID = map;
}

bool AntiCheat::Initialized()
{
    if (!m_Initialized || m_Player->GetMapId() != oldMapID)
    {
        m_Initialized = true;
        SetOldMoveInfo(false);
        SetStoredMoveInfo(false);

        for (uint8 i = 0; i < MAX_MOVE_TYPE; ++i)
            AllowedSpeed[i] = m_Player->GetSpeed(UnitMoveType(i));

        return false;
    }

    return true;
}

bool AntiCheat::SetOldMoveInfo(bool value)
{
    oldMoveInfo = newMoveInfo;
    oldMapID = m_Player->GetMapId();

    OldServerSpeed = GetServerSpeed(false);

    return value;
}

bool AntiCheat::SetStoredMoveInfo(bool value)
{
    storedMoveInfo = newMoveInfo;
    storedMapID = m_Player->GetMapId();
    return value;
}

bool AntiCheat::IsMoving(MovementInfo& moveInfo)
{
    return moveInfo.HasMovementFlag(MOVEFLAG_FORWARD) ||
        moveInfo.HasMovementFlag(MOVEFLAG_BACKWARD) ||
        moveInfo.HasMovementFlag(MOVEFLAG_STRAFE_LEFT) ||
        moveInfo.HasMovementFlag(MOVEFLAG_STRAFE_RIGHT) ||
        moveInfo.HasMovementFlag(MOVEFLAG_FALLING) ||
        moveInfo.HasMovementFlag(MOVEFLAG_FALLINGFAR) ||
        moveInfo.HasMovementFlag(MOVEFLAG_ASCENDING) ||
        moveInfo.HasMovementFlag(MOVEFLAG_SAFE_FALL);
}

bool AntiCheat::IsMoving()
{
    return IsMoving(newMoveInfo) || IsMoving(oldMoveInfo);
}

bool AntiCheat::isFlying(MovementInfo& moveInfo)
{
    return moveInfo.HasMovementFlag(MOVEFLAG_FLYING) || moveInfo.HasMovementFlag(MOVEFLAG_FLYING2);
}

bool AntiCheat::isFlying()
{
    return isFlying(newMoveInfo) || isFlying(oldMoveInfo);
}

bool AntiCheat::isWalking(MovementInfo& moveInfo)
{
    return moveInfo.HasMovementFlag(MOVEFLAG_WALK_MODE);
}

bool AntiCheat::isWalking()
{
    return isWalking(newMoveInfo) || isWalking(oldMoveInfo);
}

bool AntiCheat::isFalling(MovementInfo& moveInfo)
{
    return moveInfo.HasMovementFlag(MOVEFLAG_FALLING) || moveInfo.HasMovementFlag(MOVEFLAG_FALLINGFAR);
}

bool AntiCheat::isFalling()
{
    return isFalling(newMoveInfo) || isFalling(oldMoveInfo);
}

bool AntiCheat::isTransport(MovementInfo& moveInfo)
{
    return moveInfo.HasMovementFlag(MOVEFLAG_ONTRANSPORT);
}

bool AntiCheat::isTransport()
{
    return isTransport(newMoveInfo) || isTransport(oldMoveInfo);
}

bool AntiCheat::isSwimming(MovementInfo& moveInfo)
{
    return moveInfo.HasMovementFlag(MOVEFLAG_SWIMMING);
}

bool AntiCheat::isSwimming()
{
    return isSwimming(newMoveInfo) || isSwimming(oldMoveInfo) || m_Player->IsInWater();
}

bool AntiCheat::verifyTransportCoords(MovementInfo& moveInfo)
{
    return !(std::abs(moveInfo.GetTransportPos()->x) > 100 || std::abs(moveInfo.GetTransportPos()->y) > 100 || std::abs(moveInfo.GetTransportPos()->z) > 100);
}

bool AntiCheat::verifyTransportCoords()
{
    return verifyTransportCoords(newMoveInfo) || verifyTransportCoords(oldMoveInfo);
}

float AntiCheat::GetDistOrTransportDist()
{
    return GetDistOrTransportDist(isFlying());
}

float AntiCheat::GetDistOrTransportDist(bool threed)
{
    return isTransport(newMoveInfo) && isTransport(oldMoveInfo) ? GetTransportDist(threed) : GetDistance(threed);
}

float AntiCheat::GetDistanceZ()
{
    return newMoveInfo.GetPos()->z - oldMoveInfo.GetPos()->z;
}

float AntiCheat::GetDistance()
{
    return GetDistance(isFlying());
}

float AntiCheat::GetDistance(bool threed)
{
    return threed ? GetDistance3D() : GetDistance2D();
}

float AntiCheat::GetDistance2D()
{
    return
        sqrt(pow(newMoveInfo.GetPos()->x - oldMoveInfo.GetPos()->x, 2) +
            pow(newMoveInfo.GetPos()->y - oldMoveInfo.GetPos()->y, 2));
}

float AntiCheat::GetDistance3D()
{
    return
        sqrt(pow(newMoveInfo.GetPos()->x - oldMoveInfo.GetPos()->x, 2) +
            pow(newMoveInfo.GetPos()->y - oldMoveInfo.GetPos()->y, 2) +
            pow(newMoveInfo.GetPos()->z - oldMoveInfo.GetPos()->z, 2));
}

float AntiCheat::GetTransportDist()
{
    return GetTransportDist(isFlying());
}

float AntiCheat::GetTransportDist(bool threed)
{
    return threed ? GetTransportDist3D() : GetTransportDist2D();
}

float AntiCheat::GetTransportDist2D()
{
    return
        sqrt(pow(newMoveInfo.GetTransportPos()->x - oldMoveInfo.GetTransportPos()->x, 2) +
            pow(newMoveInfo.GetTransportPos()->y - oldMoveInfo.GetTransportPos()->y, 2));
}

float AntiCheat::GetTransportDist3D()
{
    return
        sqrt(pow(newMoveInfo.GetTransportPos()->x - oldMoveInfo.GetTransportPos()->x, 2) +
            pow(newMoveInfo.GetTransportPos()->y - oldMoveInfo.GetTransportPos()->y, 2) +
            pow(newMoveInfo.GetTransportPos()->z - oldMoveInfo.GetTransportPos()->z, 2));
}

float AntiCheat::GetTransportDistZ()
{
    return newMoveInfo.GetTransportPos()->z - oldMoveInfo.GetTransportPos()->z;
}

float AntiCheat::GetServerSpeed(bool includeold)
{
    bool back = newMoveInfo.HasMovementFlag(MOVEFLAG_BACKWARD);
    float speed = AllowedSpeed[back ? MOVE_RUN_BACK : MOVE_RUN];

    if (isFlying(newMoveInfo))
        speed = AllowedSpeed[back ? MOVE_FLIGHT_BACK : MOVE_FLIGHT];
    else if (isSwimming(newMoveInfo))
        speed = AllowedSpeed[back ? MOVE_SWIM_BACK : MOVE_SWIM];
    else if (isWalking(newMoveInfo))
        speed = AllowedSpeed[MOVE_WALK];

    return includeold ? std::max(OldServerSpeed, speed) : speed;
}


float AntiCheat::GetAllowedDistance()
{
    return GetDiffInSec() * GetServerSpeed();
}

uint32 AntiCheat::GetDiff()
{
    uint32 t1 = newMoveInfo.GetTime();
    uint32 t2 = oldMoveInfo.GetTime();

    return std::max(uint32(1), std::max(t1, t2) - std::min(t1, t2));
}

float AntiCheat::GetDiffInSec()
{
    return GetDiff() / 1000.f;
}

float AntiCheat::GetVirtualDiffInSec()
{
    if (IsMoving(newMoveInfo) && !IsMoving(oldMoveInfo))
        return 1.f;

    return GetDiffInSec();
}
