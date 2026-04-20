#include "gamecontroller.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QQueue>
#include <QRandomGenerator>
#include <QtMath>

#include <limits>

namespace {

class Skill {
public:
    virtual ~Skill() = default;
    virtual QString name() const = 0;
    virtual void cast(GameController::RuntimeUnit &caster, QVector<GameController::RuntimeUnit> &units) const = 0;
};

class WarriorSkill final : public Skill {
public:
    QString name() const override { return "Shield Break"; }
    void cast(GameController::RuntimeUnit &caster, QVector<GameController::RuntimeUnit> &units) const override
    {
        int best = -1;
        int bestHp = -1;
        for (int i = 0; i < units.size(); ++i) {
            const auto &u = units[i];
            if (!u.alive || u.data.owner == caster.data.owner) {
                continue;
            }
            if (u.hp > bestHp) {
                bestHp = u.hp;
                best = i;
            }
        }
        if (best >= 0) {
            auto &target = units[best];
            const int dmg = static_cast<int>(caster.data.atk * 2.0 * caster.skillDamageMultiplier);
            target.hp -= dmg;
            target.stunnedTicks = 10;
        }
    }
};

class MageSkill final : public Skill {
public:
    QString name() const override { return "Arc Beam"; }
    void cast(GameController::RuntimeUnit &caster, QVector<GameController::RuntimeUnit> &units) const override
    {
        int targetRow = -1;
        int bestDist = std::numeric_limits<int>::max();
        for (const auto &u : units) {
            if (!u.alive || u.data.owner == caster.data.owner) {
                continue;
            }
            const int d = qAbs(u.data.x - caster.data.x) + qAbs(u.data.y - caster.data.y);
            if (d < bestDist) {
                bestDist = d;
                targetRow = u.data.y;
            }
        }
        if (targetRow < 0) {
            return;
        }
        for (auto &u : units) {
            if (!u.alive || u.data.owner == caster.data.owner) {
                continue;
            }
            if (u.data.y == targetRow) {
                const int dmg = static_cast<int>(caster.data.atk * 1.5 * caster.skillDamageMultiplier);
                u.hp -= dmg;
            }
        }
    }
};

class PriestSkill final : public Skill {
public:
    QString name() const override { return "Sanctuary"; }
    void cast(GameController::RuntimeUnit &caster, QVector<GameController::RuntimeUnit> &units) const override
    {
        for (auto &u : units) {
            if (!u.alive || u.data.owner != caster.data.owner) {
                continue;
            }
            const int heal = 80 + caster.healBonus;
            u.hp = qMin(u.data.maxHp, u.hp + heal);
        }
    }
};

class RangerSkill final : public Skill {
public:
    QString name() const override { return "Rapid Volley"; }
    void cast(GameController::RuntimeUnit &caster, QVector<GameController::RuntimeUnit> &units) const override
    {
        int best = -1;
        double bestDist = std::numeric_limits<double>::max();
        for (int i = 0; i < units.size(); ++i) {
            const auto &u = units[i];
            if (!u.alive || u.data.owner == caster.data.owner) {
                continue;
            }
            const double dx = u.data.x - caster.data.x;
            const double dy = u.data.y - caster.data.y;
            const double dist = std::sqrt(dx * dx + dy * dy);
            if (dist < bestDist) {
                bestDist = dist;
                best = i;
            }
        }
        if (best >= 0) {
            auto &t = units[best];
            const int dmg = static_cast<int>(caster.data.atk * 1.2 * caster.skillDamageMultiplier);
            t.hp -= dmg;
            t.hp -= dmg;
        }
    }
};

class GuardianSkill final : public Skill {
public:
    QString name() const override { return "Bulwark"; }
    void cast(GameController::RuntimeUnit &caster, QVector<GameController::RuntimeUnit> &) const override
    {
        caster.hp = qMin(caster.data.maxHp, caster.hp + 140);
        caster.armor += 8;
    }
};

const Skill *skillForHero(const QString &heroId)
{
    static WarriorSkill warrior;
    static MageSkill mage;
    static PriestSkill priest;
    static RangerSkill ranger;
    static GuardianSkill guardian;

    if (heroId == "warrior") {
        return &warrior;
    }
    if (heroId == "mage") {
        return &mage;
    }
    if (heroId == "priest") {
        return &priest;
    }
    if (heroId == "ranger") {
        return &ranger;
    }
    return &guardian;
}

QString ownerName(Owner owner)
{
    return owner == Owner::Player ? "Player" : "Enemy";
}

QString stateName(UnitState state)
{
    switch (state) {
    case UnitState::Idle:
        return "Idle";
    case UnitState::Moving:
        return "Moving";
    case UnitState::Attacking:
        return "Attacking";
    case UnitState::Casting:
        return "Casting";
    case UnitState::Dead:
        return "Dead";
    }
    return "Idle";
}

}

GameController::GameController(QObject *parent)
    : QObject(parent)
{
    m_board.resize(m_rows * m_cols);
    m_bench.resize(m_benchSize);
    m_equipmentCatalog = defaultEquipmentCatalog();

    connect(&m_tickTimer, &QTimer::timeout, this, [this]() {
        if (m_phase == Phase::Prep) {
            updatePrepTimer();
        } else if (m_phase == Phase::Combat) {
            updateCombatTick();
        }
    });
    m_tickTimer.start(100);

    newGame();
}

int GameController::rows() const { return m_rows; }
int GameController::cols() const { return m_cols; }
int GameController::benchSize() const { return m_benchSize; }

int GameController::playerHp() const { return m_playerHp; }
int GameController::gold() const { return m_gold; }
int GameController::level() const { return m_level; }
int GameController::populationCap() const { return m_populationCap; }
int GameController::deployedCount() const
{
    int c = 0;
    for (const auto &slot : m_board) {
        if (slot.has_value()) {
            ++c;
        }
    }
    return c;
}

int GameController::round() const { return m_round; }
QString GameController::phase() const
{
    switch (m_phase) {
    case Phase::Prep:
        return "Prep";
    case Phase::Combat:
        return "Combat";
    case Phase::Resolve:
        return "Resolve";
    }
    return "Resolve";
}
int GameController::prepSecondsLeft() const { return m_prepSecondsLeft; }

void GameController::emitAllSignals()
{
    emit boardChanged();
    emit benchChanged();
    emit shopChanged();
    emit equipmentChanged();
    emit gameStateChanged();
}

void GameController::newGame()
{
    m_board.fill(std::nullopt);
    m_bench.fill(std::nullopt);
    m_shop.clear();
    m_equipmentPool.clear();
    m_runtimeUnits.clear();

    m_playerHp = 100;
    m_gold = 14;
    m_level = 1;
    m_populationCap = 4;
    m_round = 1;
    m_phase = Phase::Prep;
    m_prepSecondsLeft = 20;
    m_winStreak = 0;
    m_loseStreak = 0;

    m_bench[0] = makeHero("warrior", Owner::Player);
    m_bench[1] = makeHero("mage", Owner::Player);
    m_bench[2] = makeHero("priest", Owner::Player);

    rollShop();
    emitAllSignals();
}

void GameController::rollShop()
{
    m_shop.clear();
    const auto ids = allHeroIds();
    for (int i = 0; i < 5; ++i) {
        const int pick = QRandomGenerator::global()->bounded(ids.size());
        m_shop.push_back(makeHero(ids[pick], Owner::Player));
    }
    emit shopChanged();
}

bool GameController::hasBenchSpace() const
{
    for (const auto &slot : m_bench) {
        if (!slot.has_value()) {
            return true;
        }
    }
    return false;
}

int GameController::findEmptyBench() const
{
    for (int i = 0; i < m_bench.size(); ++i) {
        if (!m_bench[i].has_value()) {
            return i;
        }
    }
    return -1;
}

bool GameController::isPlayerHalf(int boardIndex) const
{
    return idxToY(boardIndex) >= m_rows / 2;
}

bool GameController::canDeployTo(int boardIndex) const
{
    return boardIndex >= 0 && boardIndex < m_board.size() && isPlayerHalf(boardIndex);
}

void GameController::moveUnit(const QString &fromArea, int fromIndex, const QString &toArea, int toIndex)
{
    if (m_phase != Phase::Prep) {
        return;
    }
    if (fromArea == toArea && fromIndex == toIndex) {
        return;
    }

    std::optional<UnitData> *fromSlot = nullptr;
    std::optional<UnitData> *toSlot = nullptr;

    if (fromArea == "bench" && fromIndex >= 0 && fromIndex < m_bench.size()) {
        fromSlot = &m_bench[fromIndex];
    } else if (fromArea == "board" && fromIndex >= 0 && fromIndex < m_board.size()) {
        fromSlot = &m_board[fromIndex];
    }

    if (toArea == "bench" && toIndex >= 0 && toIndex < m_bench.size()) {
        toSlot = &m_bench[toIndex];
    } else if (toArea == "board" && toIndex >= 0 && toIndex < m_board.size()) {
        if (!canDeployTo(toIndex)) {
            return;
        }
        if (fromArea == "bench" && deployedCount() >= m_populationCap && !m_board[toIndex].has_value()) {
            return;
        }
        toSlot = &m_board[toIndex];
    }

    if (!fromSlot || !toSlot || !fromSlot->has_value()) {
        return;
    }

    std::swap(*fromSlot, *toSlot);

    for (int i = 0; i < m_board.size(); ++i) {
        if (m_board[i].has_value()) {
            m_board[i]->x = idxToX(i);
            m_board[i]->y = idxToY(i);
        }
    }
    emit boardChanged();
    emit benchChanged();
    emit gameStateChanged();
}

bool GameController::mergeIfPossible(const UnitData &candidate)
{
    int count = 0;
    QVector<QPair<bool, int>> positions;

    for (int i = 0; i < m_bench.size(); ++i) {
        if (m_bench[i].has_value() && m_bench[i]->heroId == candidate.heroId && m_bench[i]->star == 1) {
            ++count;
            positions.push_back({false, i});
        }
    }

    for (int i = 0; i < m_board.size(); ++i) {
        if (m_board[i].has_value() && m_board[i]->heroId == candidate.heroId && m_board[i]->star == 1) {
            ++count;
            positions.push_back({true, i});
        }
    }

    if (count < 3) {
        return false;
    }

    for (int k = 0; k < 3; ++k) {
        const auto [isBoard, idx] = positions[k];
        if (isBoard) {
            m_board[idx] = std::nullopt;
        } else {
            m_bench[idx] = std::nullopt;
        }
    }

    UnitData upgraded = makeHero(candidate.heroId, Owner::Player);
    upgraded.star = 2;
    upgraded.maxHp = static_cast<int>(upgraded.maxHp * 1.7);
    upgraded.hp = upgraded.maxHp;
    upgraded.atk = static_cast<int>(upgraded.atk * 1.6);

    int benchPos = findEmptyBench();
    if (benchPos >= 0) {
        m_bench[benchPos] = upgraded;
    }

    emit combatLog(QString("3x %1 merged into 2-star").arg(candidate.heroName));
    return true;
}

void GameController::buyUnit(int shopIndex)
{
    if (m_phase != Phase::Prep || shopIndex < 0 || shopIndex >= m_shop.size()) {
        return;
    }
    const int cost = 3;
    if (m_gold < cost || !hasBenchSpace()) {
        return;
    }
    m_gold -= cost;
    const int idx = findEmptyBench();
    if (idx >= 0) {
        m_bench[idx] = m_shop[shopIndex];
        mergeIfPossible(*m_bench[idx]);
    }
    const auto ids = allHeroIds();
    const int pick = QRandomGenerator::global()->bounded(ids.size());
    m_shop[shopIndex] = makeHero(ids[pick], Owner::Player);

    emit benchChanged();
    emit boardChanged();
    emit shopChanged();
    emit gameStateChanged();
}

void GameController::refreshShop()
{
    if (m_phase != Phase::Prep || m_gold < 2) {
        return;
    }
    m_gold -= 2;
    rollShop();
    emit gameStateChanged();
}

void GameController::upgradePopulation()
{
    if (m_phase != Phase::Prep) {
        return;
    }
    const int cost = 4 + m_level * 2;
    if (m_gold < cost) {
        return;
    }
    m_gold -= cost;
    ++m_level;
    ++m_populationCap;
    emit gameStateChanged();
}

void GameController::equipUnit(int equipmentIndex, const QString &targetArea, int targetIndex)
{
    if (m_phase != Phase::Prep || equipmentIndex < 0 || equipmentIndex >= m_equipmentPool.size()) {
        return;
    }

    std::optional<UnitData> *slot = nullptr;
    if (targetArea == "bench" && targetIndex >= 0 && targetIndex < m_bench.size()) {
        slot = &m_bench[targetIndex];
    } else if (targetArea == "board" && targetIndex >= 0 && targetIndex < m_board.size()) {
        slot = &m_board[targetIndex];
    }

    if (!slot || !slot->has_value() || slot->value().hasEquipment) {
        return;
    }

    auto &u = slot->value();
    const auto eq = m_equipmentPool[equipmentIndex];
    u.hasEquipment = true;
    u.equipment = eq;
    u.atk += eq.atkBonus;
    u.maxHp += eq.hpBonus;
    u.hp += eq.hpBonus;
    u.maxMana = qMax(20, u.maxMana + eq.maxManaDelta);

    m_equipmentPool.removeAt(equipmentIndex);
    emit benchChanged();
    emit boardChanged();
    emit equipmentChanged();
}

int GameController::idxToX(int idx) const { return idx % m_cols; }
int GameController::idxToY(int idx) const { return idx / m_cols; }
int GameController::xyToIdx(int x, int y) const { return y * m_cols + x; }

void GameController::updatePrepTimer()
{
}

void GameController::startCombat()
{
    if (m_phase != Phase::Prep) {
        return;
    }
    m_phase = Phase::Combat;
    buildRuntimeUnits();
    spawnEnemyWave();
    applySynergyBonuses();
    emit combatLog(QString("Round %1 combat started").arg(m_round));
    emit boardChanged();
    emit gameStateChanged();
}

void GameController::buildRuntimeUnits()
{
    m_runtimeUnits.clear();
    for (int i = 0; i < m_board.size(); ++i) {
        if (!m_board[i].has_value()) {
            continue;
        }
        RuntimeUnit ru;
        ru.runtimeId = m_nextRuntimeId++;
        ru.data = *m_board[i];
        ru.data.owner = Owner::Player;
        ru.data.x = idxToX(i);
        ru.data.y = idxToY(i);
        ru.hp = ru.data.hp;
        ru.mana = 0;
        ru.attackCd = 0;
        ru.moveCd = 0;
        m_runtimeUnits.push_back(ru);
    }
}

void GameController::spawnEnemyWave()
{
    const int enemyCount = qMin(2 + m_round, 7);
    for (int i = 0; i < enemyCount; ++i) {
        const auto ids = allHeroIds();
        UnitData enemy = makeHero(ids[QRandomGenerator::global()->bounded(ids.size())], Owner::Enemy);
        const double growth = 1.0 + (m_round / 3) * 0.2;
        enemy.maxHp = static_cast<int>(enemy.maxHp * growth);
        enemy.hp = enemy.maxHp;
        enemy.atk = static_cast<int>(enemy.atk * growth);

        const int x = i % m_cols;
        const int y = i / m_cols;
        enemy.x = x;
        enemy.y = y;

        RuntimeUnit ru;
        ru.runtimeId = m_nextRuntimeId++;
        ru.data = enemy;
        ru.hp = enemy.hp;
        m_runtimeUnits.push_back(ru);
    }
}

void GameController::applySynergyBonuses()
{
    QMap<QString, int> traitCount;
    for (const auto &u : m_runtimeUnits) {
        if (!u.alive || u.data.owner != Owner::Player) {
            continue;
        }
        for (const auto &trait : u.data.traits) {
            traitCount[trait] += 1;
        }
    }

    for (auto &u : m_runtimeUnits) {
        if (u.data.owner != Owner::Player) {
            continue;
        }

        if (traitCount["Warrior"] >= 2) {
            u.data.maxHp += 80;
            u.hp += 80;
            u.armor += 4;
        }
        if (traitCount["Warrior"] >= 4) {
            u.data.maxHp += 120;
            u.hp += 120;
            u.armor += 4;
        }
        if (traitCount["Guardian"] >= 2) {
            u.data.maxHp += 100;
            u.hp += 100;
        }
        if (traitCount["Mage"] >= 3) {
            u.skillDamageMultiplier = 2.0;
        }
        if (traitCount["Ranger"] >= 2) {
            u.doubleStrikeChance = 0.35;
        }
        if (traitCount["Healer"] >= 2) {
            u.healBonus = 40;
        }
        if (traitCount["Assassin"] >= 2) {
            u.critChance = 0.25;
        }
    }
}

int GameController::chooseTargetIndex(int selfIndex) const
{
    const auto &self = m_runtimeUnits[selfIndex];
    int best = -1;
    double bestDist = std::numeric_limits<double>::max();

    for (int i = 0; i < m_runtimeUnits.size(); ++i) {
        const auto &other = m_runtimeUnits[i];
        if (!other.alive || other.data.owner == self.data.owner) {
            continue;
        }

        const double dx = other.data.x - self.data.x;
        const double dy = other.data.y - self.data.y;
        const double dist = std::sqrt(dx * dx + dy * dy);

        auto better = false;
        if (dist + 1e-6 < bestDist) {
            better = true;
        } else if (qAbs(dist - bestDist) <= 1e-6) {
            if (best < 0 || other.hp > m_runtimeUnits[best].hp) {
                better = true;
            } else if (other.hp == m_runtimeUnits[best].hp) {
                if (other.data.x < m_runtimeUnits[best].data.x) {
                    better = true;
                } else if (other.data.x == m_runtimeUnits[best].data.x && other.data.y > m_runtimeUnits[best].data.y) {
                    better = true;
                }
            }
        }

        if (better) {
            bestDist = dist;
            best = i;
        }
    }
    return best;
}

int GameController::chooseNextStep(int selfIndex, int targetIndex) const
{
    const auto &self = m_runtimeUnits[selfIndex];
    const auto &target = m_runtimeUnits[targetIndex];

    const int total = m_rows * m_cols;
    QVector<int> prev(total, -1);
    QVector<bool> visited(total, false);
    QQueue<int> q;

    const int start = xyToIdx(self.data.x, self.data.y);
    const int goal = xyToIdx(target.data.x, target.data.y);

    auto blocked = [&](int idx) {
        const int x = idxToX(idx);
        const int y = idxToY(idx);
        for (int i = 0; i < m_runtimeUnits.size(); ++i) {
            if (!m_runtimeUnits[i].alive || i == selfIndex || i == targetIndex) {
                continue;
            }
            if (m_runtimeUnits[i].data.x == x && m_runtimeUnits[i].data.y == y) {
                return true;
            }
        }
        return false;
    };

    visited[start] = true;
    q.enqueue(start);

    const QVector<QPair<int, int>> dirs = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    while (!q.isEmpty()) {
        const int cur = q.dequeue();
        if (cur == goal) {
            break;
        }
        const int cx = idxToX(cur);
        const int cy = idxToY(cur);
        for (const auto &[dx, dy] : dirs) {
            const int nx = cx + dx;
            const int ny = cy + dy;
            if (nx < 0 || ny < 0 || nx >= m_cols || ny >= m_rows) {
                continue;
            }
            const int nidx = xyToIdx(nx, ny);
            if (visited[nidx] || blocked(nidx)) {
                continue;
            }
            visited[nidx] = true;
            prev[nidx] = cur;
            q.enqueue(nidx);
        }
    }

    if (!visited[goal]) {
        return -1;
    }

    int cur = goal;
    int step = goal;
    while (prev[cur] != -1 && prev[cur] != start) {
        cur = prev[cur];
        step = cur;
    }
    if (prev[cur] == start) {
        step = cur;
    }
    return step;
}

void GameController::performAttack(int attackerIndex, int targetIndex)
{
    auto &attacker = m_runtimeUnits[attackerIndex];
    auto &target = m_runtimeUnits[targetIndex];
    if (!attacker.alive || !target.alive) {
        return;
    }

    int damage = attacker.data.atk;
    if (QRandomGenerator::global()->generateDouble() < attacker.critChance) {
        damage = static_cast<int>(damage * 1.7);
    }
    damage = qMax(1, damage - target.armor);

    target.hp -= damage;
    target.lastAction = "Hit";
    target.actionTicks = 4;
    attacker.mana = qMin(attacker.data.maxMana, attacker.mana + 10);

    if (QRandomGenerator::global()->generateDouble() < attacker.doubleStrikeChance) {
        target.hp -= damage;
    }
    attacker.lastAction = "Attack";
    attacker.actionTicks = 5;

    int atkTicks = attacker.data.attackIntervalTicks;
    if (attacker.data.hasEquipment) {
        atkTicks = static_cast<int>(atkTicks / attacker.data.equipment.attackSpeedMultiplier);
    }
    attacker.attackCd = qMax(4, atkTicks);
}

void GameController::castSkill(int casterIndex)
{
    auto &caster = m_runtimeUnits[casterIndex];
    const Skill *skill = skillForHero(caster.data.heroId);
    skill->cast(caster, m_runtimeUnits);
    caster.mana = 0;
    caster.lastAction = "Cast";
    caster.actionTicks = 8;
    emit combatLog(QString("%1 cast %2").arg(caster.data.heroName, skill->name()));
}

void GameController::checkDeaths()
{
    for (auto &u : m_runtimeUnits) {
        if (u.alive && u.hp <= 0) {
            u.alive = false;
            u.state = UnitState::Dead;
            emit combatLog(QString("%1 fell").arg(u.data.heroName));
        }
    }
}

bool GameController::sideAlive(Owner side) const
{
    for (const auto &u : m_runtimeUnits) {
        if (u.alive && u.data.owner == side) {
            return true;
        }
    }
    return false;
}

void GameController::updateCombatTick()
{
    for (int i = 0; i < m_runtimeUnits.size(); ++i) {
        auto &u = m_runtimeUnits[i];
        if (!u.alive) {
            continue;
        }

        if (u.actionTicks > 0) {
            --u.actionTicks;
            if (u.actionTicks == 0) {
                u.lastAction.clear();
            }
        }
        u.currentTargetRuntimeId = -1;

        if (u.stunnedTicks > 0) {
            --u.stunnedTicks;
            u.state = UnitState::Idle;
            continue;
        }

        if (u.attackCd > 0) {
            --u.attackCd;
        }
        if (u.moveCd > 0) {
            --u.moveCd;
        }

        const int targetIndex = chooseTargetIndex(i);
        if (targetIndex < 0) {
            u.state = UnitState::Idle;
            continue;
        }
        u.currentTargetRuntimeId = m_runtimeUnits[targetIndex].runtimeId;

        auto &target = m_runtimeUnits[targetIndex];
        const double dx = target.data.x - u.data.x;
        const double dy = target.data.y - u.data.y;
        const double dist = std::sqrt(dx * dx + dy * dy);

        if (dist <= u.data.range + 1e-6) {
            u.state = UnitState::Attacking;
            if (u.attackCd <= 0) {
                performAttack(i, targetIndex);
                if (u.mana >= u.data.maxMana) {
                    u.state = UnitState::Casting;
                    castSkill(i);
                }
            }
        } else {
            u.state = UnitState::Moving;
            if (u.moveCd <= 0) {
                const int step = chooseNextStep(i, targetIndex);
                if (step >= 0) {
                    u.data.x = idxToX(step);
                    u.data.y = idxToY(step);
                    u.lastAction = "Move";
                    u.actionTicks = 3;
                }
                u.moveCd = u.data.moveIntervalTicks;
            }
        }
    }

    checkDeaths();
    emit boardChanged();

    const bool playerAlive = sideAlive(Owner::Player);
    const bool enemyAlive = sideAlive(Owner::Enemy);
    if (!playerAlive || !enemyAlive) {
        endCombat(playerAlive && !enemyAlive);
    }
}

void GameController::endCombat(bool playerWon)
{
    m_phase = Phase::Resolve;

    if (playerWon) {
        m_winStreak += 1;
        m_loseStreak = 0;
        if (QRandomGenerator::global()->generateDouble() < 0.7) {
            m_equipmentPool.push_back(m_equipmentCatalog[QRandomGenerator::global()->bounded(m_equipmentCatalog.size())]);
        }
    } else {
        m_loseStreak += 1;
        m_winStreak = 0;
        const int hpLoss = qMax(3, m_round + 1);
        m_playerHp -= hpLoss;
    }

    const int baseReward = playerWon ? 9 : 5;
    const int interest = qMin(5, m_gold / 10);
    const int streakBonus = playerWon ? qMin(4, m_winStreak / 2) : qMin(3, m_loseStreak / 2);
    m_gold += baseReward + interest + streakBonus;

    m_runtimeUnits.clear();
    if (m_playerHp > 0) {
        ++m_round;
    }

    m_phase = Phase::Prep;
    m_prepSecondsLeft = 20;
    m_prepTickAccumulator = 0;
    rollShop();

    emit equipmentChanged();
    emit gameStateChanged();
    emit boardChanged();
}

QVariantMap GameController::unitToMap(const UnitData &u, bool forCombat, int hp, int mana,
                                      UnitState state, const QString &lastAction,
                                      int targetX, int targetY) const
{
    QVariantMap m;
    m["heroId"] = u.heroId;
    m["name"] = u.heroName;
    m["owner"] = ownerName(u.owner);
    m["star"] = u.star;
    m["atk"] = u.atk;
    m["range"] = u.range;
    m["maxHp"] = u.maxHp;
    m["hp"] = forCombat ? hp : u.hp;
    m["maxMana"] = u.maxMana;
    m["mana"] = forCombat ? mana : u.mana;
    m["x"] = u.x;
    m["y"] = u.y;
    m["traits"] = u.traits;
    m["equipment"] = u.hasEquipment ? u.equipment.name : "None";
    m["state"] = stateName(state);
    m["lastAction"] = lastAction;
    m["targetX"] = targetX;
    m["targetY"] = targetY;
    return m;
}

QVariantList GameController::boardCells() const
{
    QVariantList out;
    out.reserve(m_rows * m_cols);

    if (m_phase == Phase::Combat) {
        for (int idx = 0; idx < m_rows * m_cols; ++idx) {
            QVariantMap cell;
            const int x = idxToX(idx);
            const int y = idxToY(idx);
            cell["index"] = idx;
            cell["x"] = x;
            cell["y"] = y;
            cell["playerHalf"] = y >= m_rows / 2;

            QVariantMap unit;
            for (const auto &ru : m_runtimeUnits) {
                if (!ru.alive) {
                    continue;
                }
                if (ru.data.x == x && ru.data.y == y) {
                    int targetX = -1;
                    int targetY = -1;
                    if (ru.currentTargetRuntimeId >= 0) {
                        for (const auto &target : m_runtimeUnits) {
                            if (!target.alive) {
                                continue;
                            }
                            if (target.runtimeId == ru.currentTargetRuntimeId) {
                                targetX = target.data.x;
                                targetY = target.data.y;
                                break;
                            }
                        }
                    }
                    unit = unitToMap(ru.data, true, ru.hp, ru.mana, ru.state, ru.lastAction, targetX, targetY);
                    break;
                }
            }
            cell["unit"] = unit;
            out.push_back(cell);
        }
        return out;
    }

    for (int idx = 0; idx < m_rows * m_cols; ++idx) {
        QVariantMap cell;
        cell["index"] = idx;
        cell["x"] = idxToX(idx);
        cell["y"] = idxToY(idx);
        cell["playerHalf"] = idxToY(idx) >= m_rows / 2;
        QVariantMap unit;
        if (m_board[idx].has_value()) {
            unit = unitToMap(*m_board[idx], false, 0, 0);
        }
        cell["unit"] = unit;
        out.push_back(cell);
    }
    return out;
}

QVariantList GameController::benchCells() const
{
    QVariantList out;
    out.reserve(m_bench.size());
    for (int i = 0; i < m_bench.size(); ++i) {
        QVariantMap m;
        m["index"] = i;
        QVariantMap unit;
        if (m_bench[i].has_value()) {
            unit = unitToMap(*m_bench[i], false, 0, 0);
        }
        m["unit"] = unit;
        out.push_back(m);
    }
    return out;
}

QVariantList GameController::shopSlots() const
{
    QVariantList out;
    for (int i = 0; i < m_shop.size(); ++i) {
        QVariantMap m;
        m["index"] = i;
        m["cost"] = 3;
        m["unit"] = unitToMap(m_shop[i], false, 0, 0);
        out.push_back(m);
    }
    return out;
}

QVariantList GameController::equipmentSlots() const
{
    QVariantList out;
    for (int i = 0; i < m_equipmentPool.size(); ++i) {
        QVariantMap m;
        m["index"] = i;
        m["name"] = m_equipmentPool[i].name;
        m["atkBonus"] = m_equipmentPool[i].atkBonus;
        m["hpBonus"] = m_equipmentPool[i].hpBonus;
        m["speed"] = m_equipmentPool[i].attackSpeedMultiplier;
        m["manaDelta"] = m_equipmentPool[i].maxManaDelta;
        out.push_back(m);
    }
    return out;
}

QVariantList GameController::traitSummary() const
{
    QMap<QString, int> count;
    for (const auto &slot : m_board) {
        if (!slot.has_value()) {
            continue;
        }
        for (const auto &trait : slot->traits) {
            count[trait] += 1;
        }
    }

    QVariantList out;
    const auto appendTrait = [&](const QString &trait, const QString &threshold, const QString &effect) {
        QVariantMap m;
        m["trait"] = trait;
        m["count"] = count[trait];
        m["threshold"] = threshold;
        m["effect"] = effect;
        out.push_back(m);
    };

    appendTrait("Warrior", "2/4", "+HP aura + armor");
    appendTrait("Guardian", "2", "+team HP aura");
    appendTrait("Mage", "3", "Skill damage x2");
    appendTrait("Ranger", "2", "Chance to attack twice");
    appendTrait("Healer", "2", "Skill healing increased");
    appendTrait("Assassin", "2", "Critical hit chance");

    return out;
}

void GameController::saveGame(const QString &path)
{
    writeJson(path);
}

void GameController::loadGame(const QString &path)
{
    readJson(path);
    emitAllSignals();
}

void GameController::writeJson(const QString &path) const
{
    QJsonObject root;
    root["playerHp"] = m_playerHp;
    root["gold"] = m_gold;
    root["level"] = m_level;
    root["populationCap"] = m_populationCap;
    root["round"] = m_round;
    root["phase"] = phase();
    root["prepSecondsLeft"] = m_prepSecondsLeft;

    auto unitToJson = [](const UnitData &u) {
        QJsonObject o;
        o["heroId"] = u.heroId;
        o["owner"] = ownerName(u.owner);
        o["star"] = u.star;
        o["hp"] = u.hp;
        o["maxHp"] = u.maxHp;
        o["atk"] = u.atk;
        o["range"] = u.range;
        o["maxMana"] = u.maxMana;
        o["mana"] = u.mana;
        o["x"] = u.x;
        o["y"] = u.y;
        o["hasEquipment"] = u.hasEquipment;
        if (u.hasEquipment) {
            QJsonObject e;
            e["name"] = u.equipment.name;
            e["atkBonus"] = u.equipment.atkBonus;
            e["hpBonus"] = u.equipment.hpBonus;
            e["attackSpeedMultiplier"] = u.equipment.attackSpeedMultiplier;
            e["maxManaDelta"] = u.equipment.maxManaDelta;
            o["equipment"] = e;
        }
        QJsonArray traits;
        for (const auto &t : u.traits) {
            traits.append(t);
        }
        o["traits"] = traits;
        return o;
    };

    QJsonArray boardArray;
    for (const auto &slot : m_board) {
        if (slot.has_value()) {
            boardArray.append(unitToJson(*slot));
        } else {
            boardArray.append(QJsonValue::Null);
        }
    }
    root["board"] = boardArray;

    QJsonArray benchArray;
    for (const auto &slot : m_bench) {
        if (slot.has_value()) {
            benchArray.append(unitToJson(*slot));
        } else {
            benchArray.append(QJsonValue::Null);
        }
    }
    root["bench"] = benchArray;

    QJsonArray shopArray;
    for (const auto &u : m_shop) {
        shopArray.append(unitToJson(u));
    }
    root["shop"] = shopArray;

    QJsonArray equipmentArray;
    for (const auto &e : m_equipmentPool) {
        QJsonObject o;
        o["name"] = e.name;
        o["atkBonus"] = e.atkBonus;
        o["hpBonus"] = e.hpBonus;
        o["attackSpeedMultiplier"] = e.attackSpeedMultiplier;
        o["maxManaDelta"] = e.maxManaDelta;
        equipmentArray.append(o);
    }
    root["equipmentPool"] = equipmentArray;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        return;
    }
    f.write(QJsonDocument(root).toJson());
}

void GameController::readJson(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }

    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        return;
    }
    const auto root = doc.object();

    m_playerHp = root.value("playerHp").toInt(100);
    m_gold = root.value("gold").toInt(14);
    m_level = root.value("level").toInt(1);
    m_populationCap = root.value("populationCap").toInt(4);
    m_round = root.value("round").toInt(1);
    m_prepSecondsLeft = root.value("prepSecondsLeft").toInt(20);
    m_phase = Phase::Prep;

    auto jsonToUnit = [](const QJsonObject &o) {
        UnitData u = makeHero(o.value("heroId").toString("warrior"), o.value("owner").toString() == "Enemy" ? Owner::Enemy : Owner::Player);
        u.star = o.value("star").toInt(1);
        u.hp = o.value("hp").toInt(u.hp);
        u.maxHp = o.value("maxHp").toInt(u.maxHp);
        u.atk = o.value("atk").toInt(u.atk);
        u.range = o.value("range").toInt(u.range);
        u.maxMana = o.value("maxMana").toInt(u.maxMana);
        u.mana = o.value("mana").toInt(0);
        u.x = o.value("x").toInt(-1);
        u.y = o.value("y").toInt(-1);
        u.hasEquipment = o.value("hasEquipment").toBool(false);
        if (u.hasEquipment && o.contains("equipment")) {
            auto e = o.value("equipment").toObject();
            u.equipment.name = e.value("name").toString();
            u.equipment.atkBonus = e.value("atkBonus").toInt();
            u.equipment.hpBonus = e.value("hpBonus").toInt();
            u.equipment.attackSpeedMultiplier = e.value("attackSpeedMultiplier").toDouble(1.0);
            u.equipment.maxManaDelta = e.value("maxManaDelta").toInt();
        }
        u.traits.clear();
        for (const auto &v : o.value("traits").toArray()) {
            u.traits.push_back(v.toString());
        }
        return u;
    };

    m_board.fill(std::nullopt);
    const auto boardArray = root.value("board").toArray();
    for (int i = 0; i < boardArray.size() && i < m_board.size(); ++i) {
        if (boardArray[i].isObject()) {
            m_board[i] = jsonToUnit(boardArray[i].toObject());
        }
    }

    m_bench.fill(std::nullopt);
    const auto benchArray = root.value("bench").toArray();
    for (int i = 0; i < benchArray.size() && i < m_bench.size(); ++i) {
        if (benchArray[i].isObject()) {
            m_bench[i] = jsonToUnit(benchArray[i].toObject());
        }
    }

    m_shop.clear();
    for (const auto &v : root.value("shop").toArray()) {
        if (v.isObject()) {
            m_shop.push_back(jsonToUnit(v.toObject()));
        }
    }
    if (m_shop.isEmpty()) {
        rollShop();
    }

    m_equipmentPool.clear();
    for (const auto &v : root.value("equipmentPool").toArray()) {
        if (!v.isObject()) {
            continue;
        }
        Equipment e;
        const auto o = v.toObject();
        e.name = o.value("name").toString();
        e.atkBonus = o.value("atkBonus").toInt();
        e.hpBonus = o.value("hpBonus").toInt();
        e.attackSpeedMultiplier = o.value("attackSpeedMultiplier").toDouble(1.0);
        e.maxManaDelta = o.value("maxManaDelta").toInt();
        m_equipmentPool.push_back(e);
    }
}
