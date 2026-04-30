import React from "react";
import { displaySubsystem, formatValue, valueFromYamcs } from "../lib/model";
import { Card, EmptyState, EnvBanner, Icon, PageHeader, Sidebar, StatusDot, SubsysPill } from "../primitives";
import { T } from "../tokens";

function buildInitialArgs(command) {
  return Object.fromEntries(
    (command.argument || []).map((argument) => [
      argument.name,
      argument.initialValue ?? (argument.type?.engType === "boolean" ? false : ""),
    ]),
  );
}

function coerceArgumentValue(argument, value) {
  const type = argument.type?.engType;
  if (type === "integer") {
    return value === "" ? null : Number(value);
  }
  if (type === "float") {
    return value === "" ? null : Number(value);
  }
  if (type === "boolean") {
    return Boolean(value);
  }
  return value;
}

function CommandModal({ command, onClose, onSend, issuingCommand, error }) {
  const [args, setArgs] = React.useState(() => buildInitialArgs(command));
  const unsupported = (command.argument || []).some((argument) => ["aggregate", "array", "integer[]", "enumeration[]"].includes(argument.type?.engType));
  return (
    <div style={{ position: "fixed", inset: 0, background: "rgba(29,29,31,0.3)", backdropFilter: "blur(10px)", display: "flex", alignItems: "center", justifyContent: "center", zIndex: 20 }}>
      <div style={{ width: 640, maxWidth: "calc(100vw - 48px)", maxHeight: "calc(100vh - 48px)", overflow: "auto", background: T.bg, borderRadius: 18, boxShadow: T.shadowModal }}>
        <div style={{ padding: "20px 24px 16px", borderBottom: `1px solid ${T.divider}`, display: "flex", alignItems: "flex-start", justifyContent: "space-between", gap: 16 }}>
          <div>
            <div style={{ display: "flex", alignItems: "center", gap: 8, marginBottom: 6 }}>
              <SubsysPill name={displaySubsystem(command.qualifiedName)} />
              <span style={{ fontFamily: T.fontMono, fontSize: 11, color: T.fgMuted }}>{command.qualifiedName}</span>
            </div>
            <div style={{ fontSize: 19, fontWeight: 600 }}>{command.qualifiedName.split("/").pop()}</div>
            <div style={{ fontSize: 12.5, color: T.fgMuted, marginTop: 4 }}>{command.shortDescription || "Issue this command through the Yamcs processor API."}</div>
          </div>
          <button onClick={onClose} style={{ appearance: "none", border: "none", background: "transparent", color: T.fgMuted, fontSize: 24, cursor: "pointer" }}>×</button>
        </div>
        <div style={{ padding: 24, display: "grid", gap: 16 }}>
          {unsupported ? (
            <Card padding={18} style={{ background: T.redTint, borderColor: "transparent" }}>
              <div style={{ fontSize: 13, fontWeight: 600, marginBottom: 4 }}>This command needs a richer argument editor</div>
              <div style={{ fontSize: 12.5, color: T.fgMuted, lineHeight: 1.5 }}>
                Aggregate and array arguments are detected from the mission database, but this first implementation only auto-renders scalar editors.
              </div>
            </Card>
          ) : null}
          {(command.argument || []).map((argument) => {
            const type = argument.type?.engType;
            const enumValues = argument.type?.enumValue || [];
            return (
              <label key={argument.name} style={{ display: "grid", gap: 6 }}>
                <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between" }}>
                  <span style={{ fontSize: 12.5, fontWeight: 600 }}>{argument.name}</span>
                  <span style={{ fontFamily: T.fontMono, fontSize: 11, color: T.fgMuted }}>{type || "string"}</span>
                </div>
                {enumValues.length ? (
                  <select value={args[argument.name] ?? ""} onChange={(event) => setArgs((current) => ({ ...current, [argument.name]: event.target.value }))} style={{ border: `1px solid ${T.border}`, borderRadius: 10, padding: "10px 12px", background: T.bgAlt }}>
                    <option value="">Select…</option>
                    {enumValues.map((value) => (
                      <option key={value.label} value={value.label}>{value.label}</option>
                    ))}
                  </select>
                ) : type === "boolean" ? (
                  <button onClick={() => setArgs((current) => ({ ...current, [argument.name]: !current[argument.name] }))} type="button" style={{ width: 84, padding: "10px 12px", borderRadius: 999, border: `1px solid ${T.border}`, background: args[argument.name] ? T.blueTint : T.bgAlt, color: args[argument.name] ? T.blue : T.fgMuted }}>
                    {args[argument.name] ? "True" : "False"}
                  </button>
                ) : (
                  <input value={args[argument.name] ?? ""} onChange={(event) => setArgs((current) => ({ ...current, [argument.name]: event.target.value }))} style={{ border: `1px solid ${T.border}`, borderRadius: 10, padding: "10px 12px", background: T.bgAlt }} />
                )}
              </label>
            );
          })}
          {error ? (
            <div style={{ fontSize: 12.5, color: T.red }}>{error}</div>
          ) : null}
          <div style={{ display: "flex", justifyContent: "flex-end", gap: 10 }}>
            <button onClick={onClose} style={{ padding: "10px 16px", borderRadius: 999, border: `1px solid ${T.border}`, background: T.bg, cursor: "pointer" }}>Cancel</button>
            <button
              disabled={unsupported || issuingCommand}
              onClick={() => {
                const payload = Object.fromEntries(
                  (command.argument || []).map((argument) => [argument.name, coerceArgumentValue(argument, args[argument.name])]),
                );
                onSend(payload);
              }}
              style={{ padding: "10px 16px", borderRadius: 999, border: "none", background: T.blue, color: "#fff", cursor: "pointer" }}
            >
              {issuingCommand ? "Sending…" : "Send Command"}
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}

export function CommandsScreen({
  context,
  navigate,
  route,
  commandGroups,
  commands,
  commandHistory,
  issueCommand,
  issuingCommand,
}) {
  const [query, setQuery] = React.useState("");
  const [selectedCommand, setSelectedCommand] = React.useState(null);
  const [submitError, setSubmitError] = React.useState("");
  const normalized = query.trim().toLowerCase();
  const filteredGroups = commandGroups
    .map((group) => ({
      ...group,
      items: group.items.filter((command) => `${command.qualifiedName} ${command.shortDescription || ""}`.toLowerCase().includes(normalized)),
    }))
    .filter((group) => group.items.length);

  return (
    <div style={{ width: "100vw", height: "100vh", display: "flex", flexDirection: "column", background: T.bgAlt, color: T.fg, overflow: "hidden" }}>
      <EnvBanner mode="LIVE" text={context ? `${context.instance}/${context.processor} commanding` : "Discovering commands"} />
      <div style={{ display: "flex", flex: 1, minHeight: 0 }}>
        <Sidebar active={route} onNavigate={navigate} />
        <div style={{ flex: 1, display: "flex", flexDirection: "column", minWidth: 0 }}>
          <PageHeader
            title="Commands"
            subtitle="Mission database-driven command browser, auto-generated argument forms, and live command history"
            right={<div style={{ fontSize: 12.5, color: T.fgMuted }}>{commands.length} discovered commands</div>}
          />
          <div style={{ flex: 1, display: "grid", gridTemplateColumns: "1.15fr 0.85fr", gap: 16, padding: "20px 32px 28px", minHeight: 0 }}>
            <Card padding={0} style={{ display: "flex", flexDirection: "column", overflow: "hidden", minHeight: 0 }}>
              <div style={{ padding: "14px 16px", borderBottom: `1px solid ${T.divider}` }}>
                <div style={{ display: "flex", alignItems: "center", gap: 8, padding: "8px 12px", borderRadius: 10, background: T.bgAlt }}>
                  <Icon name="search" size={14} color={T.fgMuted} />
                  <input value={query} onChange={(event) => setQuery(event.target.value)} placeholder="Search commands…" style={{ flex: 1, border: "none", background: "transparent", outline: "none" }} />
                </div>
              </div>
              <div style={{ flex: 1, overflow: "auto", padding: 12 }}>
                {filteredGroups.length ? (
                  filteredGroups.map((group) => (
                    <div key={group.id} style={{ marginBottom: 14 }}>
                      <div style={{ padding: "8px 10px", display: "flex", alignItems: "center", justifyContent: "space-between" }}>
                        <SubsysPill name={group.label} />
                        <span style={{ fontSize: 11, color: T.fgMuted }}>{group.items.length}</span>
                      </div>
                      <div style={{ display: "grid", gap: 8 }}>
                        {group.items.map((command) => (
                          <button
                            key={command.qualifiedName}
                            onClick={() => {
                              setSubmitError("");
                              setSelectedCommand(command);
                            }}
                            style={{ appearance: "none", border: `1px solid ${T.border}`, background: T.bg, borderRadius: 12, padding: "14px 16px", textAlign: "left", cursor: "pointer" }}
                          >
                            <div style={{ display: "flex", alignItems: "center", gap: 8, marginBottom: 6 }}>
                              <Icon name="terminal" size={14} color={T.blue} />
                              <span style={{ fontSize: 12.5, fontWeight: 600 }}>{command.qualifiedName.split("/").pop()}</span>
                              {command.argument?.length ? <span style={{ marginLeft: "auto", fontSize: 10.5, color: T.fgMuted, fontFamily: T.fontMono }}>{command.argument.length} args</span> : null}
                            </div>
                            <div style={{ fontSize: 11.5, color: T.fgMuted, lineHeight: 1.45 }}>{command.shortDescription || command.qualifiedName}</div>
                          </button>
                        ))}
                      </div>
                    </div>
                  ))
                ) : (
                  <EmptyState title="No matching commands" detail="Adjust the search query to see command definitions discovered from the mission database." />
                )}
              </div>
            </Card>
            <Card padding={0} style={{ display: "flex", flexDirection: "column", overflow: "hidden", minHeight: 0 }}>
              <div style={{ padding: "14px 18px", borderBottom: `1px solid ${T.divider}` }}>
                <div style={{ fontSize: 13, fontWeight: 600 }}>Command History</div>
                <div style={{ fontSize: 11.5, color: T.fgMuted, marginTop: 2 }}>Live updates from the Yamcs command stream</div>
              </div>
              <div style={{ flex: 1, overflow: "auto" }}>
                {commandHistory.length ? (
                  commandHistory.map((entry, index) => {
                    const attrs = entry.attr || [];
                    const queued = attrs.find((attribute) => attribute.name === "Acknowledge_Queued_Status")?.value?.stringValue;
                    const sent = attrs.find((attribute) => attribute.name === "Acknowledge_Sent_Status")?.value?.stringValue;
                    return (
                      <div key={`${entry.id || entry.commandName}-${index}`} style={{ padding: "14px 18px", borderBottom: `1px solid ${T.divider}` }}>
                        <div style={{ display: "flex", alignItems: "center", gap: 8, marginBottom: 6 }}>
                          <StatusDot status={sent === "OK" || queued === "OK" ? "nominal" : "warning"} size={7} />
                          <span style={{ fontSize: 12.5, fontWeight: 600 }}>{entry.commandName?.split("/").pop()}</span>
                          <span style={{ marginLeft: "auto", fontSize: 10.5, color: T.fgMuted, fontFamily: T.fontMono }}>
                            {entry.generationTime ? new Date(entry.generationTime).toISOString() : "live"}
                          </span>
                        </div>
                        <div style={{ fontSize: 11.5, color: T.fgMuted, lineHeight: 1.45 }}>
                          {entry.commandName}
                        </div>
                        {entry.assignments?.length ? (
                          <div style={{ marginTop: 8, display: "flex", flexWrap: "wrap", gap: 8 }}>
                            {entry.assignments.slice(0, 4).map((assignment) => (
                              <span key={assignment.name} style={{ padding: "4px 8px", borderRadius: 999, background: T.bgAlt, fontFamily: T.fontMono, fontSize: 10.5 }}>
                                {assignment.name}={formatValue(valueFromYamcs(assignment.value))}
                              </span>
                            ))}
                          </div>
                        ) : null}
                      </div>
                    );
                  })
                ) : (
                  <EmptyState title="No command history yet" detail="Issued and archived commands will populate this feed automatically." />
                )}
              </div>
            </Card>
          </div>
        </div>
      </div>
      {selectedCommand ? (
        <CommandModal
          command={selectedCommand}
          issuingCommand={issuingCommand}
          error={submitError}
          onClose={() => setSelectedCommand(null)}
          onSend={async (args) => {
            try {
              await issueCommand(selectedCommand.qualifiedName, args);
              setSelectedCommand(null);
            } catch (error) {
              setSubmitError(error.message);
            }
          }}
        />
      ) : null}
    </div>
  );
}
