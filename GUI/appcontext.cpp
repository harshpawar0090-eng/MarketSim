#include "AppContext.h"

#include <QStringList>
#include <exception>

#include "../App/MarketSeed.h"

namespace gui
{

    AppContext::AppContext(const AppConfig &config, QObject *parent)
        : QObject(parent),
          config_(config),
          market_(app::createSeededMarket()),
          index_(app::createMarketIndex(market_)),
          portfolio_(config.startingCash),
          execution_(market_, portfolio_, execution::LiquidityConfig(), config.riskConfig),
          broker_(execution_),
          intelligence_(market_, portfolio_, execution_.riskManager(), config.startingCash)
    {
        // First point of the portfolio history, so a later equity chart starts
        // from the real initial state rather than from the first tick.
        intelligence_.recordSnapshot();
    }

    void AppContext::advanceTick()
    {
        QString summary;
        try
        {
            market_.tick();
            index_.update(market_);
            const execution::MarketUpdateResult update = broker_.onMarketUpdate();
            intelligence_.recordSnapshot();
            ++tickCount_;

            QStringList parts;
            parts << QStringLiteral("Tick %1 advanced").arg(tickCount_);
            if (!update.triggers.empty())
            {
                parts << QStringLiteral("%1 stop order(s) triggered").arg(static_cast<qlonglong>(update.triggers.size()));
            }
            if (!update.userFills.empty())
            {
                parts << QStringLiteral("%1 fill(s)").arg(static_cast<qlonglong>(update.userFills.size()));
            }
            if (update.liquidation.marginCallDetected)
            {
                if (update.liquidation.resolved)
                {
                    parts << QStringLiteral("margin call: %1 liquidation order(s), resolved")
                                 .arg(static_cast<qlonglong>(update.liquidation.events.size()));
                }
                else
                {
                    parts << QStringLiteral("MARGIN CALL NOT RESOLVED");
                }
            }
            summary = parts.join(QStringLiteral("  \u00B7  "));
        }
        catch (const std::exception &e)
        {
            summary = QStringLiteral("Tick failed: %1").arg(QString::fromUtf8(e.what()));
        }

        lastSummary_ = summary;
        emit stateChanged();
        emit tickAdvanced(summary);
    }

} // namespace gui