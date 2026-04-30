import React from "react";
import { displaySubsystem, formatValue, severityTone, summarizeSubsystemHealth, unitFor } from "../lib/model";
import { Card, EmptyState, EnvBanner, Icon, PageHeader, SeverityBadge, Sidebar, Sparkline, StatusDot, SubsysPill } from "../primitives";
import { T } from "../tokens";

function getLiveValue(parameterId, parameterById, parameterValues) {
  const parameter = parameterById[parameterId];
  const live = parameterValues[parameterId];
  return {
    parameter,
    live,
    parsed: live?.parsedValue,
    formatted: formatValue(live?.parsedValue, parameter),
    unit: unitFor(parameter),
  };
}

export function OverviewScreen({
  context,
  navigate,
  route,
  parameterById,
  parameterValues,
  featuredParameterIds,
  events,
  currentTime,
  links,
  parameters,
  commands,
  issueCommand,
  issuingCommand,
}) {
  const featured = featuredParameterIds.map((parameterId) => getLiveValue(parameterId, parameterById, parameterValues)).filter((item) => item.parameter);
  const health = summarizeSubsystemHealth(parameters, parameterValues);
  const nextQuickCommands = commands
    .filter((command) => /safe|reset|start|stop|enable|disable|image|watchdog/i.test(command.qualifiedName))
    .filter((command) => (command.argument?.length || 0) === 0)
    .slice(0, 6);

  return (
    <div style={{ width: "100vw", height: "100vh", display: "flex", flexDirection: "column", background: T.bgAlt, color: T.fg, overflow: "hidden" }}>
      <EnvBanner mode="LIVE" text={context ? `${context.instance}/${context.processor} connected` : "Discovering Yamcs"} />
      <div style={{ display: "flex", flex: 1, minHeight: 0 }}>
        <Sidebar active={route} onNavigate={navigate} />
        <div style={{ flex: 1, display: "flex", flexDirection: "column", minWidth: 0 }}>
          <PageHeader
            title="Overview"
            subtitle="Mission-wide telemetry and operations synthesized from the active Yamcs mission database"
            right={
              <div style={{ display: "flex", alignItems: "center", gap: 14, fontSize: 12.5, color: T.fgMuted }}>
                <span style={{ fontFamily: T.fontMono }}>{currentTime ? new Date(currentTime).toISOString().replace("T", " ").replace("Z", " UTC") : "Connecting…"}</span>
                <div style={{ height: 26, width: 1, background: T.border }} />
                <span>{links.filter((link) => link.status === "OK").length} live links</span>
              </div>
            }
          />
          <div style={{ flex: 1, padding: "16px 24px 20px", display: "grid", gridTemplateRows: "170px 1fr 220px", gap: 14, minHeight: 0 }}>
            <div style={{ display: "grid", gridTemplateColumns: "repeat(4, 1fr)", gap: 14 }}>
              {featured.slice(0, 4).map((item) => (
                <Card key={item.parameter.qualifiedName} padding={16} style={{ display: "flex", flexDirection: "column", gap: 8 }}>
                  <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between" }}>
                    <SubsysPill name={displaySubsystem(item.parameter.qualifiedName)} />
                    <StatusDot status={severityTone(item.live?.monitoringResult)} size={7} pulse={severityTone(item.live?.monitoringResult) === "warning"} />
                  </div>
                  <div style={{ fontSize: 12, color: T.fgMuted, fontWeight: 500 }}>{item.parameter.shortDescription || item.parameter.qualifiedName.split("/").pop()}</div>
                  <div style={{ display: "flex", alignItems: "baseline", gap: 6 }}>
                    <div style={{ fontFamily: T.fontMono, fontSize: 30, fontWeight: 600, letterSpacing: -0.6 }}>{item.formatted}</div>
                    <div style={{ fontFamily: T.fontMono, fontSize: 12, color: T.fgMuted }}>{item.unit}</div>
                  </div>
                  <div style={{ fontSize: 11, color: T.fgMuted, fontFamily: T.fontMono, overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
                    {item.parameter.qualifiedName}
                  </div>
                </Card>
              ))}
            </div>
            <div style={{ display: "grid", gridTemplateColumns: "320px 1fr 360px", gap: 14, minHeight: 0 }}>
              <Card padding={18} style={{ display: "flex", flexDirection: "column", gap: 14 }}>
                <div>
                  <div style={{ fontSize: 13, fontWeight: 600 }}>Spacecraft State</div>
                  <div style={{ fontSize: 11.5, color: T.fgMuted, marginTop: 2 }}>Discovered from active parameters and links</div>
                </div>
                <div style={{ display: "grid", gap: 10 }}>
                  {featured.slice(0, 6).map((item) => (
                    <div key={item.parameter.qualifiedName} style={{ padding: "12px 14px", background: T.bgSubtle, borderRadius: 10 }}>
                      <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", gap: 8 }}>
                        <div style={{ fontSize: 11.5, color: T.fgMuted, minWidth: 0, overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
                          {item.parameter.shortDescription || item.parameter.qualifiedName.split("/").pop()}
                        </div>
                        <StatusDot status={severityTone(item.live?.monitoringResult)} size={6} />
                      </div>
                      <div style={{ display: "flex", alignItems: "baseline", gap: 4, marginTop: 5 }}>
                        <span style={{ fontFamily: T.fontMono, fontSize: 18, fontWeight: 600 }}>{item.formatted}</span>
                        <span style={{ fontFamily: T.fontMono, fontSize: 11, color: T.fgMuted }}>{item.unit}</span>
                      </div>
                    </div>
                  ))}
                </div>
              </Card>
              <Card padding={18} style={{ display: "flex", flexDirection: "column", minHeight: 0 }}>
                <div style={{ marginBottom: 12 }}>
                  <div style={{ fontSize: 13, fontWeight: 600 }}>Priority Telemetry</div>
                  <div style={{ fontSize: 11.5, color: T.fgMuted, marginTop: 2 }}>Autoselected featured channels from the MDB</div>
                </div>
                <div style={{ display: "grid", gridTemplateColumns: "repeat(2, 1fr)", gap: 12, flex: 1, minHeight: 0 }}>
                  {featured.slice(0, 6).map((item, index) => {
                    const numeric = typeof item.parsed === "number" ? item.parsed : 0;
                    const points = Array.from({ length: 18 }, (_, pointIndex) => numeric * (0.98 + (pointIndex % 4) * 0.01));
                    return (
                      <div key={item.parameter.qualifiedName} style={{ border: `1px solid ${T.border}`, borderRadius: 12, padding: 14 }}>
                        <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", gap: 8 }}>
                          <SubsysPill name={displaySubsystem(item.parameter.qualifiedName)} />
                          <span style={{ fontSize: 10.5, color: T.fgMuted, fontFamily: T.fontMono }}>#{index + 1}</span>
                        </div>
                        <div style={{ fontSize: 12, fontWeight: 600, marginTop: 8, minWidth: 0, overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
                          {item.parameter.qualifiedName.split("/").pop()}
                        </div>
                        <div style={{ display: "flex", alignItems: "baseline", gap: 4, marginTop: 4 }}>
                          <span style={{ fontFamily: T.fontMono, fontSize: 22, fontWeight: 600 }}>{item.formatted}</span>
                          <span style={{ fontFamily: T.fontMono, fontSize: 11, color: T.fgMuted }}>{item.unit}</span>
                        </div>
                        {typeof item.parsed === "number" ? (
                          <div style={{ marginTop: 10 }}>
                            <Sparkline points={points} color={severityTone(item.live?.monitoringResult) === "warning" ? T.amber : T.blue} width={220} height={42} />
                          </div>
                        ) : null}
                      </div>
                    );
                  })}
                </div>
              </Card>
              <Card padding={0} style={{ display: "flex", flexDirection: "column", overflow: "hidden", minHeight: 0 }}>
                <div style={{ padding: "14px 18px", borderBottom: `1px solid ${T.divider}` }}>
                  <div style={{ fontSize: 13, fontWeight: 600 }}>Recent Events</div>
                  <div style={{ fontSize: 11.5, color: T.fgMuted, marginTop: 2 }}>Live Yamcs event stream and archive tail</div>
                </div>
                <div style={{ flex: 1, overflow: "auto" }}>
                  {events.length ? (
                    events.slice(0, 12).map((event, index) => (
                      <div key={`${event.seqNumber || index}-${event.generationTime || index}`} style={{ padding: "12px 18px", borderBottom: `1px solid ${T.divider}` }}>
                        <div style={{ display: "flex", alignItems: "center", gap: 8, marginBottom: 6 }}>
                          <SeverityBadge level={event.severity || "INFO"} />
                          <span style={{ fontSize: 10.5, color: T.fgMuted, fontFamily: T.fontMono }}>
                            {event.generationTime ? new Date(event.generationTime).toISOString() : "live"}
                          </span>
                        </div>
                        <div style={{ fontSize: 12.5, fontWeight: 600, color: T.fg }}>{event.type || "Event"}</div>
                        <div style={{ fontSize: 11.5, color: T.fgMuted, marginTop: 3, lineHeight: 1.45 }}>{event.message}</div>
                      </div>
                    ))
                  ) : (
                    <EmptyState title="No events yet" detail="Live and archived events will appear here as soon as Yamcs receives them." />
                  )}
                </div>
              </Card>
            </div>
            <div style={{ display: "grid", gridTemplateColumns: "1fr 480px", gap: 14, minHeight: 0 }}>
              <Card padding={18} style={{ display: "flex", flexDirection: "column", minHeight: 0 }}>
                <div style={{ marginBottom: 12 }}>
                  <div style={{ fontSize: 13, fontWeight: 600 }}>Subsystem Health</div>
                  <div style={{ fontSize: 11.5, color: T.fgMuted, marginTop: 2 }}>Computed from acquisition and monitoring states</div>
                </div>
                <div style={{ display: "grid", gridTemplateColumns: "repeat(2, 1fr)", gap: 12 }}>
                  {health.map((item) => (
                    <div key={item.label} style={{ border: `1px solid ${T.border}`, borderRadius: 12, padding: 14 }}>
                      <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between" }}>
                        <div style={{ fontSize: 12.5, fontWeight: 600 }}>{item.label}</div>
                        <StatusDot status={item.warning ? "warning" : item.stale ? "idle" : "nominal"} size={7} pulse={Boolean(item.warning)} />
                      </div>
                      <div style={{ display: "flex", gap: 10, marginTop: 8, fontSize: 11.5, color: T.fgMuted }}>
                        <span>{item.nominal} nominal</span>
                        <span>{item.warning} warn</span>
                        <span>{item.stale} stale</span>
                      </div>
                    </div>
                  ))}
                </div>
              </Card>
              <Card padding={18} style={{ display: "flex", flexDirection: "column" }}>
                <div style={{ marginBottom: 12 }}>
                  <div style={{ fontSize: 13, fontWeight: 600 }}>Quick Commands</div>
                  <div style={{ fontSize: 11.5, color: T.fgMuted, marginTop: 2 }}>Top operational actions inferred from available commands</div>
                </div>
                <div style={{ display: "grid", gridTemplateColumns: "repeat(2, 1fr)", gap: 10 }}>
                  {nextQuickCommands.map((command) => (
                    <button
                      key={command.qualifiedName}
                      onClick={() => issueCommand(command.qualifiedName, {})}
                      disabled={issuingCommand}
                      style={{
                        appearance: "none",
                        border: `1px solid ${T.border}`,
                        background: T.bg,
                        borderRadius: 12,
                        padding: "14px 16px",
                        textAlign: "left",
                        cursor: "pointer",
                      }}
                    >
                      <div style={{ display: "flex", alignItems: "center", gap: 8, marginBottom: 6 }}>
                        <Icon name="play" size={12} color={T.blue} />
                        <SubsysPill name={displaySubsystem(command.qualifiedName)} />
                      </div>
                      <div style={{ fontSize: 12.5, fontWeight: 600 }}>{command.qualifiedName.split("/").pop()}</div>
                      <div style={{ fontSize: 11, color: T.fgMuted, marginTop: 4, lineHeight: 1.4 }}>
                        {command.shortDescription || command.qualifiedName}
                      </div>
                    </button>
                  ))}
                </div>
              </Card>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
