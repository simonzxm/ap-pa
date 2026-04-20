#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

enum class Owner {
    Player,
    Enemy
};

enum class UnitState {
    Idle,
    Moving,
    Attacking,
    Casting,
    Dead
};

struct Equipment {
    QString name;
    int atkBonus = 0;
    int hpBonus = 0;
    double attackSpeedMultiplier = 1.0;
    int maxManaDelta = 0;
};

struct UnitData {
    QString heroId;
    QString heroName;
    Owner owner = Owner::Player;
    int star = 1;

    int hp = 300;
    int maxHp = 300;
    int atk = 30;
    int range = 1;
    int maxMana = 60;
    int mana = 0;

    int attackIntervalTicks = 20;
    int moveIntervalTicks = 7;

    int x = -1;
    int y = -1;
    QStringList traits;

    bool hasEquipment = false;
    Equipment equipment;
};

QVector<QString> allHeroIds();
UnitData makeHero(const QString &heroId, Owner owner);
QVector<Equipment> defaultEquipmentCatalog();
