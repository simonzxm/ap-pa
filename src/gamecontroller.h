#pragma once

#include "unit.h"

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <optional>

class GameController : public QObject {
    Q_OBJECT
    Q_PROPERTY(int rows READ rows CONSTANT)
    Q_PROPERTY(int cols READ cols CONSTANT)
    Q_PROPERTY(int benchSize READ benchSize CONSTANT)

    Q_PROPERTY(int playerHp READ playerHp NOTIFY gameStateChanged)
    Q_PROPERTY(int gold READ gold NOTIFY gameStateChanged)
    Q_PROPERTY(int level READ level NOTIFY gameStateChanged)
    Q_PROPERTY(int populationCap READ populationCap NOTIFY gameStateChanged)
    Q_PROPERTY(int deployedCount READ deployedCount NOTIFY gameStateChanged)

    Q_PROPERTY(int round READ round NOTIFY gameStateChanged)
    Q_PROPERTY(QString phase READ phase NOTIFY gameStateChanged)
    Q_PROPERTY(int prepSecondsLeft READ prepSecondsLeft NOTIFY gameStateChanged)

    Q_PROPERTY(QVariantList boardCells READ boardCells NOTIFY boardChanged)
    Q_PROPERTY(QVariantList benchCells READ benchCells NOTIFY benchChanged)
    Q_PROPERTY(QVariantList shopSlots READ shopSlots NOTIFY shopChanged)
    Q_PROPERTY(QVariantList equipmentSlots READ equipmentSlots NOTIFY equipmentChanged)
    Q_PROPERTY(QVariantList traitSummary READ traitSummary NOTIFY gameStateChanged)

public:
    struct RuntimeUnit {
        int runtimeId = 0;
        UnitData data;
        UnitState state = UnitState::Idle;
        int hp = 0;
        int mana = 0;
        bool alive = true;
        int attackCd = 0;
        int moveCd = 0;
        int stunnedTicks = 0;
        int armor = 0;
        double skillDamageMultiplier = 1.0;
        double doubleStrikeChance = 0.0;
        double critChance = 0.0;
        int healBonus = 0;
    };

    explicit GameController(QObject *parent = nullptr);

    int rows() const;
    int cols() const;
    int benchSize() const;

    int playerHp() const;
    int gold() const;
    int level() const;
    int populationCap() const;
    int deployedCount() const;

    int round() const;
    QString phase() const;
    int prepSecondsLeft() const;

    QVariantList boardCells() const;
    QVariantList benchCells() const;
    QVariantList shopSlots() const;
    QVariantList equipmentSlots() const;
    QVariantList traitSummary() const;

    Q_INVOKABLE void newGame();
    Q_INVOKABLE void startCombat();
    Q_INVOKABLE void moveUnit(const QString &fromArea, int fromIndex, const QString &toArea, int toIndex);
    Q_INVOKABLE void buyUnit(int shopIndex);
    Q_INVOKABLE void refreshShop();
    Q_INVOKABLE void upgradePopulation();
    Q_INVOKABLE void equipUnit(int equipmentIndex, const QString &targetArea, int targetIndex);
    Q_INVOKABLE void saveGame(const QString &path);
    Q_INVOKABLE void loadGame(const QString &path);

signals:
    void boardChanged();
    void benchChanged();
    void shopChanged();
    void equipmentChanged();
    void gameStateChanged();
    void combatLog(const QString &line);

private:
    enum class Phase {
        Prep,
        Combat,
        Resolve
    };

    int m_rows = 8;
    int m_cols = 8;
    int m_benchSize = 8;

    int m_playerHp = 100;
    int m_gold = 12;
    int m_level = 1;
    int m_populationCap = 3;
    int m_round = 1;
    Phase m_phase = Phase::Prep;
    int m_prepSecondsLeft = 20;

    int m_winStreak = 0;
    int m_loseStreak = 0;
    int m_nextRuntimeId = 1;

    QVector<std::optional<UnitData>> m_board;
    QVector<std::optional<UnitData>> m_bench;
    QVector<UnitData> m_shop;
    QVector<Equipment> m_equipmentCatalog;
    QVector<Equipment> m_equipmentPool;

    QVector<RuntimeUnit> m_runtimeUnits;

    QTimer m_tickTimer;
    int m_prepTickAccumulator = 0;

    void emitAllSignals();
    void rollShop();
    bool hasBenchSpace() const;
    int findEmptyBench() const;
    bool canDeployTo(int boardIndex) const;
    bool isPlayerHalf(int boardIndex) const;

    void updatePrepTimer();
    void updateCombatTick();
    void endCombat(bool playerWon);

    void buildRuntimeUnits();
    void spawnEnemyWave();
    void applySynergyBonuses();

    int idxToX(int idx) const;
    int idxToY(int idx) const;
    int xyToIdx(int x, int y) const;

    int chooseTargetIndex(int selfIndex) const;
    int chooseNextStep(int selfIndex, int targetIndex) const;
    void performAttack(int attackerIndex, int targetIndex);
    void castSkill(int casterIndex);
    void checkDeaths();
    bool sideAlive(Owner side) const;

    bool mergeIfPossible(const UnitData &candidate);

    QVariantMap unitToMap(const UnitData &u, bool forCombat, int hp, int mana) const;
    void writeJson(const QString &path) const;
    void readJson(const QString &path);
};
