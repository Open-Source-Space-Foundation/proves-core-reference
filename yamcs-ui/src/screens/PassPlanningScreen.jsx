import React from "react";
import { Card, EmptyState, EnvBanner, PageHeader, Sidebar, StatusDot } from "../primitives";
import { T } from "../tokens";

export function PassPlanningScreen({ context, navigate, route, currentTime, links, events }) {
  const now = currentTime ? new Date(currentTime) : null;
  const liveLinks = links.filter((link) => link.status === "OK");
  return (
    <div style={{ width: "100vw", height: "100vh", display: "flex", flexDirection: "column", background: T.bgAlt, color: T.fg, overflow: "hidden" }}>
      <EnvBanner mode="LIVE" text={context ? `${context.instance} timeline and link state` : "Discovering pass context"} />
      <div style={{ display: "flex", flex: 1, minHeight: 0 }}>
        <Sidebar active={route} onNavigate={navigate} />
        <div style={{ flex: 1, display: "flex", flexDirection: "column", minWidth: 0 }}>
          <PageHeader
            title="Pass Planning"
            subtitle="Ground-contact screen implemented against generic Yamcs data sources while awaiting an orbital schedule feed"
            right={<div style={{ fontFamily: T.fontMono, fontSize: 12, color: T.fgMuted }}>{now ? now.toISOString() : "Connecting…"}</div>}
          />
          <div style={{ flex: 1, padding: "20px 32px 28px", display: "grid", gridTemplateColumns: "1fr 440px", gap: 16, minHeight: 0 }}>
            <Card padding={0} style={{ overflow: "hidden", display: "flex", flexDirection: "column" }}>
              <div style={{ padding: "16px 20px", borderBottom: `1px solid ${T.divider}` }}>
                <div style={{ fontSize: 13, fontWeight: 600 }}>Coverage & Contacts</div>
                <div style={{ fontSize: 11.5, color: T.fgMuted, marginTop: 2 }}>Live Yamcs links are shown now; orbital pass overlays can plug into the same shell later.</div>
              </div>
              <div style={{ flex: 1, background: "radial-gradient(circle at 30% 20%, #1f3968 0%, #101a31 55%, #0b1020 100%)", position: "relative", minHeight: 0 }}>
                <svg viewBox="0 0 820 520" preserveAspectRatio="none" style={{ width: "100%", height: "100%", display: "block" }}>
                  <defs>
                    <pattern id="grid" width="60" height="60" patternUnits="userSpaceOnUse">
                      <path d="M 60 0 L 0 0 0 60" fill="none" stroke="rgba(255,255,255,0.06)" strokeWidth="1" />
                    </pattern>
                  </defs>
                  <rect width="820" height="520" fill="url(#grid)" />
                  <line x1="0" x2="820" y1="260" y2="260" stroke="rgba(255,255,255,0.18)" strokeDasharray="4 6" />
                  <path d="M 70 120 Q 140 70, 210 120 Q 250 180, 210 215 Q 145 230, 90 190 Q 60 160, 70 120 Z" fill="#1c2741" />
                  <path d="M 210 220 Q 235 225, 250 270 Q 245 320, 225 350 Q 215 330, 210 275 Z" fill="#1c2741" />
                  <path d="M 400 120 Q 450 105, 490 125 Q 470 155, 420 150 Z" fill="#1c2741" />
                  <path d="M 430 170 Q 485 170, 520 215 Q 525 285, 490 335 Q 450 345, 425 280 Q 415 225, 430 170 Z" fill="#1c2741" />
                  <path d="M 520 110 Q 650 95, 710 145 Q 700 190, 610 195 Q 545 185, 520 150 Z" fill="#1c2741" />
                  <path d="M 645 320 Q 715 320, 740 360 Q 725 390, 660 388 Q 635 366, 645 320 Z" fill="#1c2741" />
                  <circle cx="270" cy="200" r="86" fill="rgba(0,113,227,0.08)" stroke="rgba(0,113,227,0.45)" strokeDasharray="4 6" />
                  <path d="M 60 365 Q 220 155, 465 210 Q 620 245, 760 142" fill="none" stroke="rgba(255,255,255,0.75)" strokeDasharray="6 6" />
                  <circle cx="270" cy="200" r="6" fill="#0071e3" />
                  <circle cx="270" cy="200" r="14" fill="none" stroke="rgba(0,113,227,0.4)" />
                </svg>
                <div style={{ position: "absolute", top: 16, left: 16, background: "rgba(12,18,34,0.82)", border: "1px solid rgba(255,255,255,0.08)", borderRadius: 10, padding: "10px 12px", color: "#d9e3f5" }}>
                  <div style={{ fontSize: 10, textTransform: "uppercase", letterSpacing: 0.4, color: "#91a6cc", marginBottom: 6 }}>Active Links</div>
                  {liveLinks.length ? liveLinks.map((link) => (
                    <div key={link.name} style={{ display: "flex", alignItems: "center", gap: 8, padding: "3px 0", fontSize: 11.5 }}>
                      <StatusDot status="nominal" size={6} />
                      <span>{link.name}</span>
                      <span style={{ marginLeft: "auto", color: "#91a6cc", fontFamily: T.fontMono }}>{link.status}</span>
                    </div>
                  )) : <div style={{ fontSize: 11.5, color: "#91a6cc" }}>No live ground links yet</div>}
                </div>
                <div style={{ position: "absolute", bottom: 16, right: 16, background: "rgba(12,18,34,0.82)", border: "1px solid rgba(255,255,255,0.08)", borderRadius: 10, padding: "8px 10px", color: "#d9e3f5", fontFamily: T.fontMono, fontSize: 10.5 }}>
                  Mission time {now ? now.toISOString().slice(11, 19) : "--:--:--"} UTC
                </div>
              </div>
            </Card>
            <div style={{ display: "grid", gridTemplateRows: "0.95fr 1.05fr", gap: 16, minHeight: 0 }}>
              <Card padding={0} style={{ overflow: "hidden", display: "flex", flexDirection: "column" }}>
                <div style={{ padding: "14px 18px", borderBottom: `1px solid ${T.divider}` }}>
                  <div style={{ fontSize: 13, fontWeight: 600 }}>Ground Links</div>
                  <div style={{ fontSize: 11.5, color: T.fgMuted, marginTop: 2 }}>Realtime connectivity and transport health from Yamcs</div>
                </div>
                <div style={{ flex: 1, overflow: "auto" }}>
                  {links.length ? links.map((link) => (
                    <div key={link.name} style={{ padding: "14px 18px", borderBottom: `1px solid ${T.divider}` }}>
                      <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
                        <StatusDot status={link.status === "OK" ? "nominal" : "warning"} size={7} pulse={link.status !== "OK"} />
                        <span style={{ fontSize: 12.5, fontWeight: 600 }}>{link.name}</span>
                        <span style={{ marginLeft: "auto", fontFamily: T.fontMono, fontSize: 11, color: T.fgMuted }}>{link.status}</span>
                      </div>
                      <div style={{ fontSize: 11.5, color: T.fgMuted, marginTop: 6 }}>{link.type}</div>
                      {link.extra ? (
                        <div style={{ marginTop: 8, display: "flex", flexWrap: "wrap", gap: 8 }}>
                          {Object.entries(link.extra).map(([name, value]) => (
                            <span key={name} style={{ padding: "4px 8px", borderRadius: 999, background: T.bgAlt, fontFamily: T.fontMono, fontSize: 10.5 }}>
                              {name}: {value}
                            </span>
                          ))}
                        </div>
                      ) : null}
                    </div>
                  )) : <EmptyState title="No link records" detail="Yamcs will surface link objects here as soon as the instance advertises them." />}
                </div>
              </Card>
              <Card padding={0} style={{ overflow: "hidden", display: "flex", flexDirection: "column" }}>
                <div style={{ padding: "14px 18px", borderBottom: `1px solid ${T.divider}` }}>
                  <div style={{ fontSize: 13, fontWeight: 600 }}>Operational Checklist</div>
                  <div style={{ fontSize: 11.5, color: T.fgMuted, marginTop: 2 }}>This can be driven from future mission procedures; for now it reflects the live event tail.</div>
                </div>
                <div style={{ flex: 1, overflow: "auto" }}>
                  {events.length ? events.slice(0, 8).map((event, index) => (
                    <div key={`${event.seqNumber || index}-${event.generationTime || index}`} style={{ padding: "12px 18px", borderBottom: `1px solid ${T.divider}`, display: "flex", gap: 10 }}>
                      <div style={{ width: 18, height: 18, borderRadius: 5, border: `1.5px solid ${T.borderStrong}`, background: T.bg, flexShrink: 0 }} />
                      <div>
                        <div style={{ fontSize: 12.5, color: T.fg }}>{event.type || "Event"}</div>
                        <div style={{ fontSize: 11.5, color: T.fgMuted, marginTop: 3, lineHeight: 1.45 }}>{event.message}</div>
                      </div>
                    </div>
                  )) : <EmptyState title="No recent operations" detail="When the event stream becomes active, those actions appear here as a checklist-style feed." />}
                </div>
              </Card>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
