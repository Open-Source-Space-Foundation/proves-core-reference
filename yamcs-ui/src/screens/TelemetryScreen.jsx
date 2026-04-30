import React from "react";
import { formatValue, guessViz, isAggregateParameter, unitFor, valueFromYamcs } from "../lib/model";
import { Card, EmptyState, EnvBanner, Icon, PageHeader, Sidebar, StatusDot, SubsysPill } from "../primitives";
import { T } from "../tokens";

function NumericStats({ history, unit }) {
  if (!history.length) {
    return null;
  }
  const values = history.map((point) => point.value).filter((value) => Number.isFinite(value));
  if (!values.length) {
    return null;
  }
  const avg = values.reduce((sum, value) => sum + value, 0) / values.length;
  const variance = values.reduce((sum, value) => sum + (value - avg) ** 2, 0) / values.length;
  const sigma = Math.sqrt(variance);
  const items = [
    ["Min", Math.min(...values)],
    ["Avg", avg],
    ["Max", Math.max(...values)],
    ["σ", sigma],
  ];
  return (
    <div style={{ display: "grid", gridTemplateColumns: "repeat(4, 1fr)", borderTop: `1px solid ${T.divider}` }}>
      {items.map(([label, value], index) => (
        <div key={label} style={{ padding: "12px 18px", borderRight: index < items.length - 1 ? `1px solid ${T.divider}` : "none" }}>
          <div style={{ fontSize: 10.5, color: T.fgMuted, fontWeight: 600, letterSpacing: 0.3, textTransform: "uppercase" }}>{label}</div>
          <div style={{ display: "flex", alignItems: "baseline", gap: 4, marginTop: 3 }}>
            <span style={{ fontFamily: T.fontMono, fontSize: 16, fontWeight: 600 }}>{formatValue(value)}</span>
            <span style={{ fontFamily: T.fontMono, fontSize: 10.5, color: T.fgMuted }}>{unit}</span>
          </div>
        </div>
      ))}
    </div>
  );
}

function ChannelTree({ groups, selectedId, onSelect, valuesById }) {
  const [query, setQuery] = React.useState("");
  const normalized = query.trim().toLowerCase();
  return (
    <div style={{ background: T.bg, border: `1px solid ${T.border}`, borderRadius: 14, display: "flex", flexDirection: "column", overflow: "hidden", minHeight: 0 }}>
      <div style={{ padding: "12px 14px", borderBottom: `1px solid ${T.divider}` }}>
        <div style={{ display: "flex", alignItems: "center", gap: 8, padding: "6px 10px", background: T.bgAlt, borderRadius: 8, fontSize: 12.5 }}>
          <Icon name="search" size={14} color={T.fgMuted} />
          <input value={query} onChange={(event) => setQuery(event.target.value)} placeholder="Search channels…" style={{ flex: 1, border: "none", background: "transparent", outline: "none", color: T.fg }} />
        </div>
      </div>
      <div style={{ flex: 1, overflow: "auto", padding: "6px 0" }}>
        {groups.map((group) => {
          const items = group.items.filter((parameter) => {
            if (!normalized) {
              return true;
            }
            return `${parameter.qualifiedName} ${parameter.shortDescription || ""}`.toLowerCase().includes(normalized);
          });
          if (!items.length) {
            return null;
          }
          return (
            <div key={group.id}>
              <div style={{ padding: "6px 14px", display: "flex", alignItems: "center", gap: 8, fontSize: 12 }}>
                <Icon name="chevronDown" size={11} color={T.fgMuted} />
                <SubsysPill name={group.label} />
              </div>
              {items.map((parameter) => {
                const selected = parameter.qualifiedName === selectedId;
                const live = valuesById[parameter.qualifiedName];
                return (
                  <button
                    key={parameter.qualifiedName}
                    onClick={() => onSelect(parameter.qualifiedName)}
                    style={{
                      appearance: "none",
                      border: "none",
                      width: "100%",
                      background: selected ? T.blueTint : "transparent",
                      borderLeft: selected ? `2px solid ${T.blue}` : "2px solid transparent",
                      padding: "6px 14px 6px 36px",
                      display: "flex",
                      alignItems: "center",
                      gap: 8,
                      cursor: "pointer",
                      textAlign: "left",
                    }}
                  >
                    <span style={{ flex: 1, fontSize: 11.5, color: selected ? T.blue : T.fg, fontWeight: selected ? 600 : 500, overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
                      {parameter.qualifiedName.split("/").pop()}
                    </span>
                    <span style={{ fontFamily: T.fontMono, fontSize: 11, color: T.fgMuted, fontVariantNumeric: "tabular-nums" }}>
                      {formatValue(live?.parsedValue, parameter)} {unitFor(parameter)}
                    </span>
                    <StatusDot status={live?.monitoringResult?.startsWith("WARN") ? "warning" : "nominal"} size={6} />
                  </button>
                );
              })}
            </div>
          );
        })}
      </div>
    </div>
  );
}

function LineViz({ history, unit }) {
  if (!history.length) {
    return <EmptyState title="No archived samples" detail="This parameter does not have recent history available in Yamcs yet." />;
  }
  const values = history.map((point) => point.value).filter((value) => Number.isFinite(value));
  const min = Math.min(...values);
  const max = Math.max(...values);
  const range = max - min || 1;
  const w = 860;
  const h = 310;
  const path = values
    .map((value, index) => {
      const x = (index / Math.max(1, values.length - 1)) * w;
      const y = h - ((value - min) / range) * h;
      return `${index === 0 ? "M" : "L"}${x.toFixed(1)},${y.toFixed(1)}`;
    })
    .join(" ");
  return (
    <div style={{ height: "100%", padding: "16px 22px" }}>
      <svg viewBox={`0 0 ${w} ${h}`} preserveAspectRatio="none" style={{ width: "100%", height: "100%", display: "block" }}>
        {[0.2, 0.4, 0.6, 0.8].map((p) => <line key={p} x1={0} x2={w} y1={p * h} y2={p * h} stroke={T.divider} strokeWidth={1} />)}
        <path d={`${path} L${w},${h} L0,${h} Z`} fill={T.blue} opacity={0.08} />
        <path d={path} fill="none" stroke={T.blue} strokeWidth={2} strokeLinecap="round" strokeLinejoin="round" />
        <text x={8} y={16} fontSize="11" fill={T.fgMuted} fontFamily={T.fontMono}>{min.toFixed(2)} {unit}</text>
        <text x={8} y={h - 8} fontSize="11" fill={T.fgMuted} fontFamily={T.fontMono}>{max.toFixed(2)} {unit}</text>
      </svg>
    </div>
  );
}

function BarViz({ liveValue, unit }) {
  const numeric = Number(liveValue) || 0;
  const clamped = Math.max(0, Math.min(100, numeric));
  return (
    <div style={{ height: "100%", padding: "26px 32px", display: "flex", flexDirection: "column", justifyContent: "center", gap: 22 }}>
      <div style={{ display: "flex", alignItems: "baseline", gap: 10 }}>
        <div style={{ fontFamily: T.fontMono, fontSize: 72, fontWeight: 600, letterSpacing: -2 }}>{formatValue(clamped)}</div>
        <div style={{ fontFamily: T.fontMono, fontSize: 22, color: T.fgMuted }}>{unit || "%"}</div>
      </div>
      <div style={{ display: "flex", gap: 2, height: 52 }}>
        {Array.from({ length: 40 }, (_, index) => {
          const filled = index < Math.round((clamped / 100) * 40);
          const tone = index >= 36 ? T.red : index >= 30 ? T.amber : T.green;
          return <div key={index} style={{ flex: 1, borderRadius: 2, background: filled ? tone : T.bgAlt }} />;
        })}
      </div>
    </div>
  );
}

function RadialViz({ liveValue, unit }) {
  const numeric = Number(liveValue) || 0;
  const min = Math.max(0, numeric * 0.7);
  const max = numeric * 1.3 || 1;
  const percent = (numeric - min) / (max - min || 1);
  const cx = 200;
  const cy = 200;
  const r = 130;
  const startAngle = -210;
  const endAngle = 30;
  const sweep = endAngle - startAngle;
  const angle = startAngle + percent * sweep;
  const polar = (degrees, radius) => {
    const radians = (degrees * Math.PI) / 180;
    return [cx + radius * Math.cos(radians), cy + radius * Math.sin(radians)];
  };
  const arc = (a0, a1) => {
    const [x0, y0] = polar(a0, r);
    const [x1, y1] = polar(a1, r);
    const large = a1 - a0 > 180 ? 1 : 0;
    return `M ${x0} ${y0} A ${r} ${r} 0 ${large} 1 ${x1} ${y1}`;
  };
  const [needleX, needleY] = polar(angle, r - 10);
  return (
    <div style={{ height: "100%", display: "flex", alignItems: "center", justifyContent: "center", padding: 16 }}>
      <svg width={410} height={340} viewBox="0 0 400 380">
        <path d={arc(startAngle, endAngle)} fill="none" stroke={T.bgAlt} strokeWidth={28} strokeLinecap="round" />
        <path d={arc(startAngle, angle)} fill="none" stroke={T.blue} strokeWidth={10} strokeLinecap="round" />
        <line x1={cx} y1={cy} x2={needleX} y2={needleY} stroke={T.fg} strokeWidth={3} strokeLinecap="round" />
        <circle cx={cx} cy={cy} r={10} fill={T.bg} stroke={T.fg} strokeWidth={2} />
        <text x={cx} y={cy + 60} textAnchor="middle" fontSize="42" fontWeight="600" fill={T.fg} fontFamily={T.fontMono}>{formatValue(numeric)}</text>
        <text x={cx} y={cy + 82} textAnchor="middle" fontSize="13" fill={T.fgMuted} fontFamily={T.fontMono}>{unit}</text>
      </svg>
    </div>
  );
}

function DialViz({ liveValue, unit }) {
  const numeric = Math.max(0, Number(liveValue) || 0);
  const max = numeric * 2.5 || 5;
  const cx = 180;
  const cy = 180;
  const r = 120;
  const start = 180;
  const end = 360;
  const angle = start + (numeric / max) * (end - start);
  const polar = (degrees, radius) => {
    const radians = (degrees * Math.PI) / 180;
    return [cx + radius * Math.cos(radians), cy + radius * Math.sin(radians)];
  };
  const arc = (a0, a1) => {
    const [x0, y0] = polar(a0, r);
    const [x1, y1] = polar(a1, r);
    return `M ${x0} ${y0} A ${r} ${r} 0 0 1 ${x1} ${y1}`;
  };
  const [needleX, needleY] = polar(angle, r - 2);
  return (
    <div style={{ height: "100%", display: "flex", alignItems: "center", justifyContent: "center" }}>
      <svg width={400} height={260} viewBox="0 0 360 260">
        <path d={arc(start, end)} fill="none" stroke={T.bgAlt} strokeWidth={20} strokeLinecap="round" />
        <path d={arc(start, angle)} fill="none" stroke={T.blue} strokeWidth={8} strokeLinecap="round" />
        <line x1={cx} y1={cy} x2={needleX} y2={needleY} stroke={T.fg} strokeWidth={2.5} strokeLinecap="round" />
        <circle cx={cx} cy={cy} r={5} fill={T.blue} />
        <text x={cx} y={cy + 48} textAnchor="middle" fontSize="42" fontWeight="600" fill={T.fg} fontFamily={T.fontMono}>{formatValue(numeric)}</text>
        <text x={cx} y={cy + 68} textAnchor="middle" fontSize="13" fill={T.fgMuted} fontFamily={T.fontMono}>{unit}</text>
      </svg>
    </div>
  );
}

function MultiBarViz({ parameter, liveValue }) {
  const members = parameter?.type?.member || [];
  const values = typeof liveValue === "object" && liveValue ? liveValue : {};
  const rows = members
    .map((member) => ({
      label: member.name,
      value: Number(values[member.name]),
      unit: member.type?.unitSet?.[0]?.unit || "",
    }))
    .filter((item) => Number.isFinite(item.value));
  const max = rows.length ? Math.max(...rows.map((item) => Math.abs(item.value))) || 1 : 1;
  if (!rows.length) {
    return <EmptyState title="No member values yet" detail="Yamcs has not produced an aggregate sample for this parameter." />;
  }
  return (
    <div style={{ height: "100%", padding: "24px 28px", display: "grid", gap: 18, alignContent: "center" }}>
      {rows.map((row) => (
        <div key={row.label}>
          <div style={{ display: "flex", alignItems: "baseline", justifyContent: "space-between", marginBottom: 6 }}>
            <span style={{ fontSize: 12.5, fontWeight: 600 }}>{row.label}</span>
            <span style={{ fontFamily: T.fontMono, fontSize: 12, color: T.fgMuted }}>{formatValue(row.value)} {row.unit}</span>
          </div>
          <div style={{ height: 22, background: T.bgAlt, borderRadius: 999, overflow: "hidden" }}>
            <div style={{ width: `${(Math.abs(row.value) / max) * 100}%`, height: "100%", background: T.blue, borderRadius: 999 }} />
          </div>
        </div>
      ))}
    </div>
  );
}

function DetailViz({ liveValue, parameter }) {
  const members = isAggregateParameter(parameter) && liveValue && typeof liveValue === "object" ? Object.entries(liveValue) : [];
  return (
    <div style={{ height: "100%", display: "flex", alignItems: "center", justifyContent: "center", padding: 28 }}>
      <Card padding={26} style={{ width: "100%", maxWidth: 600 }}>
        <div style={{ fontSize: 13, fontWeight: 600, marginBottom: 8 }}>Current Value</div>
        <div style={{ fontFamily: T.fontMono, fontSize: 28, fontWeight: 600 }}>{formatValue(liveValue, parameter)}</div>
        {members.length ? (
          <div style={{ marginTop: 16, display: "grid", gap: 10 }}>
            {members.map(([name, value]) => (
              <div key={name} style={{ display: "flex", justifyContent: "space-between", gap: 12, padding: "10px 12px", background: T.bgAlt, borderRadius: 10 }}>
                <span style={{ color: T.fgMuted }}>{name}</span>
                <span style={{ fontFamily: T.fontMono }}>{formatValue(value)}</span>
              </div>
            ))}
          </div>
        ) : null}
      </Card>
    </div>
  );
}

function ChannelChart({ parameter, live, history, historyLoading }) {
  if (!parameter) {
    return (
      <div style={{ background: T.bg, border: `1px solid ${T.border}`, borderRadius: 14, minHeight: 0 }}>
        <EmptyState title="Select a parameter" detail="Choose any discovered MDB parameter from the left-hand telemetry tree." />
      </div>
    );
  }
  const unit = unitFor(parameter);
  const viz = guessViz(parameter, live?.parsedValue);
  return (
    <div style={{ background: T.bg, border: `1px solid ${T.border}`, borderRadius: 14, display: "flex", flexDirection: "column", overflow: "hidden", minHeight: 0 }}>
      <div style={{ padding: "16px 22px 12px", borderBottom: `1px solid ${T.divider}`, display: "flex", alignItems: "center", justifyContent: "space-between" }}>
        <div>
          <div style={{ display: "flex", alignItems: "center", gap: 8, marginBottom: 4 }}>
            <SubsysPill name={parameter.qualifiedName.split("/").slice(2, 3)[0] || "Telemetry"} />
            <span style={{ fontSize: 11, color: T.fgMuted, fontFamily: T.fontMono }}>{parameter.qualifiedName}</span>
          </div>
          <div style={{ fontSize: 17, fontWeight: 600 }}>{parameter.shortDescription || parameter.qualifiedName.split("/").pop()}</div>
        </div>
        <div style={{ display: "flex", alignItems: "baseline", gap: 6 }}>
          <div style={{ fontFamily: T.fontMono, fontSize: 28, fontWeight: 600 }}>{formatValue(live?.parsedValue, parameter)}</div>
          <div style={{ fontFamily: T.fontMono, fontSize: 13, color: T.fgMuted }}>{unit}</div>
        </div>
      </div>
      <div style={{ padding: "10px 22px", display: "flex", alignItems: "center", justifyContent: "space-between", borderBottom: `1px solid ${T.divider}` }}>
        <div style={{ display: "inline-flex", background: T.bgAlt, borderRadius: 8, padding: 3, gap: 2 }}>
          {["Live", "1H", "6H", "1D"].map((range, index) => (
            <div key={range} style={{ padding: "4px 12px", borderRadius: 6, fontSize: 11.5, fontWeight: 500, background: index === 1 ? T.bg : "transparent", color: index === 1 ? T.fg : T.fgMuted }}>
              {range}
            </div>
          ))}
        </div>
        <div style={{ fontSize: 11, color: T.fgMuted, fontFamily: T.fontMono }}>{historyLoading ? "Loading history…" : `${history.length} archived samples`}</div>
      </div>
      <div style={{ flex: 1, minHeight: 0 }}>
        {viz === "line" ? <LineViz history={history} unit={unit} /> : null}
        {viz === "bar" ? <BarViz liveValue={live?.parsedValue} unit={unit || "%"} /> : null}
        {viz === "radial" ? <RadialViz liveValue={live?.parsedValue} unit={unit} /> : null}
        {viz === "dial" ? <DialViz liveValue={live?.parsedValue} unit={unit} /> : null}
        {viz === "multibar" ? <MultiBarViz parameter={parameter} liveValue={live?.parsedValue} /> : null}
        {viz === "detail" ? <DetailViz liveValue={live?.parsedValue} parameter={parameter} /> : null}
      </div>
      <NumericStats history={history} unit={unit} />
    </div>
  );
}

export function TelemetryScreen({
  context,
  navigate,
  route,
  parameterTree,
  parameterById,
  parameterValues,
  selectedParameterId,
  setSelectedParameterId,
  featuredParameterIds,
  history,
  historyLoading,
}) {
  const selectedParameter = selectedParameterId ? parameterById[selectedParameterId] : null;
  const selectedLive = selectedParameterId ? parameterValues[selectedParameterId] : null;
  const pinned = featuredParameterIds.slice(0, 4).map((parameterId) => ({ parameter: parameterById[parameterId], live: parameterValues[parameterId] })).filter((item) => item.parameter);
  return (
    <div style={{ width: "100vw", height: "100vh", display: "flex", flexDirection: "column", background: T.bgAlt, color: T.fg, overflow: "hidden" }}>
      <EnvBanner mode="LIVE" text={context ? `${context.instance}/${context.processor} telemetry` : "Discovering telemetry"} />
      <div style={{ display: "flex", flex: 1, minHeight: 0 }}>
        <Sidebar active={route} onNavigate={navigate} />
        <div style={{ flex: 1, display: "flex", flexDirection: "column", minWidth: 0 }}>
          <PageHeader
            title="Telemetry"
            subtitle="Every chart and tree entry is discovered live from the active Yamcs mission database"
            right={
              <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
                <button style={{ padding: "8px 14px", borderRadius: 8, border: `1px solid ${T.border}`, background: T.bg, fontSize: 12.5, color: T.fg }}>MDB-driven</button>
              </div>
            }
          />
          <div style={{ padding: "10px 32px 12px" }}>
            <div style={{ display: "grid", gridTemplateColumns: "repeat(4, 1fr)", gap: 10 }}>
              {pinned.map(({ parameter, live }) => (
                <div key={parameter.qualifiedName} style={{ background: T.bg, border: `1px solid ${T.border}`, borderRadius: 10, padding: "8px 12px", display: "flex", alignItems: "center", gap: 10 }}>
                  <div style={{ flex: 1, minWidth: 0 }}>
                    <div style={{ display: "flex", alignItems: "center", gap: 6, marginBottom: 1 }}>
                      <SubsysPill name={parameter.qualifiedName.split("/").slice(2, 3)[0] || "TLM"} />
                      <Icon name="pin" size={10} color={T.fgSubtle} />
                    </div>
                    <div style={{ fontSize: 11, color: T.fgMuted, fontWeight: 500, overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>{parameter.qualifiedName.split("/").pop()}</div>
                  </div>
                  <div style={{ display: "flex", flexDirection: "column", alignItems: "flex-end", gap: 3 }}>
                    <div style={{ display: "flex", alignItems: "baseline", gap: 4 }}>
                      <span style={{ fontFamily: T.fontMono, fontSize: 16, fontWeight: 600 }}>{formatValue(live?.parsedValue, parameter)}</span>
                      <span style={{ fontFamily: T.fontMono, fontSize: 10.5, color: T.fgMuted }}>{unitFor(parameter)}</span>
                    </div>
                    <StatusDot status={live?.monitoringResult?.startsWith("WARN") ? "warning" : "nominal"} size={6} />
                  </div>
                </div>
              ))}
            </div>
          </div>
          <div style={{ flex: 1, display: "grid", gridTemplateColumns: "320px 1fr", minHeight: 0, padding: "0 32px 28px", gap: 16 }}>
            <ChannelTree groups={parameterTree} selectedId={selectedParameterId} onSelect={setSelectedParameterId} valuesById={parameterValues} />
            <ChannelChart parameter={selectedParameter} live={selectedLive} history={history} historyLoading={historyLoading} />
          </div>
        </div>
      </div>
    </div>
  );
}
