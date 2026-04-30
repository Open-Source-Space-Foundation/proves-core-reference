import React from "react";
import { displaySubsystem } from "../lib/model";
import { Card, EmptyState, EnvBanner, Icon, PageHeader, Sidebar, StatusDot, SubsysPill } from "../primitives";
import { T } from "../tokens";

function inferServiceCards(commands, links) {
  const serviceCommands = commands.filter((command) => /deploy|bootloader|watchdog|power|radio|link|image|reset/i.test(command.qualifiedName));
  const cards = serviceCommands.slice(0, 6).map((command) => ({
    icon: /radio|link/i.test(command.qualifiedName) ? "radio" : /deploy/i.test(command.qualifiedName) ? "antenna" : "satellite",
    title: command.qualifiedName.split("/").pop(),
    subtitle: command.shortDescription || command.qualifiedName,
    command,
  }));
  if (links.length) {
    cards.unshift({
      icon: "server",
      title: "Link Diagnostics",
      subtitle: `${links.length} link objects discovered from Yamcs`,
      linkSummary: links,
    });
  }
  return cards.slice(0, 6);
}

export function GroundServicesScreen({ context, navigate, route, commands, links, issuingCommand, issueCommand }) {
  const cards = inferServiceCards(commands, links);
  return (
    <div style={{ width: "100vw", height: "100vh", display: "flex", flexDirection: "column", background: T.bgAlt, color: T.fg, overflow: "hidden" }}>
      <EnvBanner mode="LIVE" text={context ? `${context.instance} ground services` : "Discovering ground services"} />
      <div style={{ display: "flex", flex: 1, minHeight: 0 }}>
        <Sidebar active={route} onNavigate={navigate} />
        <div style={{ flex: 1, display: "flex", flexDirection: "column", minWidth: 0 }}>
          <PageHeader
            title="Ground Services"
            subtitle="Commandable ground operations panels generated from Yamcs links and mission commands"
            right={<div style={{ display: "flex", alignItems: "center", gap: 8, fontSize: 12, color: T.fgMuted }}><StatusDot status="nominal" size={7} />{links.length} active services</div>}
          />
          <div style={{ flex: 1, overflow: "auto", padding: "20px 32px 28px" }}>
            <div style={{ display: "grid", gridTemplateColumns: "repeat(3, 1fr)", gap: 16 }}>
              {cards.length ? cards.map((card) => (
                <Card key={card.title} padding={20} style={{ display: "flex", flexDirection: "column", gap: 16, minHeight: 240 }}>
                  <div style={{ display: "flex", alignItems: "flex-start", gap: 12 }}>
                    <div style={{ width: 36, height: 36, borderRadius: 10, background: T.bgAlt, display: "flex", alignItems: "center", justifyContent: "center" }}>
                      <Icon name={card.icon} size={18} color={T.fg} />
                    </div>
                    <div style={{ flex: 1, minWidth: 0 }}>
                      <div style={{ fontSize: 14, fontWeight: 600 }}>{card.title}</div>
                      <div style={{ fontSize: 12, color: T.fgMuted, marginTop: 3, lineHeight: 1.4 }}>{card.subtitle}</div>
                    </div>
                  </div>
                  <div style={{ flex: 1, display: "grid", gap: 8 }}>
                    {card.command ? (
                      <>
                        <div style={{ padding: "8px 10px", borderRadius: 8, background: T.bgAlt, fontFamily: T.fontMono, fontSize: 12, color: T.fgMuted }}>
                          {card.command.qualifiedName}
                        </div>
                        <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
                          <SubsysPill name={displaySubsystem(card.command.qualifiedName)} />
                          <span style={{ fontSize: 11, color: T.fgMuted }}>{card.command.argument?.length || 0} args</span>
                        </div>
                      </>
                    ) : null}
                    {card.linkSummary ? card.linkSummary.map((link) => (
                      <div key={link.name} style={{ display: "flex", alignItems: "center", justifyContent: "space-between", gap: 10, padding: "8px 10px", borderRadius: 8, background: T.bgAlt }}>
                        <span style={{ fontSize: 11.5 }}>{link.name}</span>
                        <span style={{ fontFamily: T.fontMono, fontSize: 11, color: T.fgMuted }}>{link.status}</span>
                      </div>
                    )) : null}
                  </div>
                  <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", gap: 12 }}>
                    {card.command ? (
                      <button onClick={() => issueCommand(card.command.qualifiedName, {})} disabled={issuingCommand || (card.command.argument?.length || 0) > 0} style={{ padding: "8px 16px", borderRadius: 999, border: "none", background: T.fg, color: "#fff", cursor: "pointer" }}>
                        {(card.command.argument?.length || 0) > 0 ? "Use Commands Screen" : issuingCommand ? "Sending…" : "Execute"}
                      </button>
                    ) : <div />}
                    <div style={{ display: "inline-flex", alignItems: "center", gap: 6, padding: "4px 10px", borderRadius: 999, background: T.blueTint, color: T.blue, fontSize: 11, fontWeight: 600, fontFamily: T.fontMono }}>
                      <StatusDot status="active" size={7} pulse />
                      <span>DISCOVERED</span>
                    </div>
                  </div>
                </Card>
              )) : <EmptyState title="No service cards inferred yet" detail="As more mission commands and links become available, this screen will populate automatically from Yamcs discovery." />}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
