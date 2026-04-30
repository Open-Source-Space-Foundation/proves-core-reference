import { T } from "./tokens";

export function Icon({ name, size = 18, color = "currentColor", strokeWidth = 1.6 }) {
  const paths = {
    home: <path d="M3 10.5L10 4l7 6.5V16a1 1 0 0 1-1 1h-3v-4H8v4H4a1 1 0 0 1-1-1v-5.5z" />,
    activity: <path d="M2 10h3l2-5 3 10 3-7 2 4h3" />,
    terminal: <><path d="M3 5h14v10H3z" /><path d="M5 8l2 2-2 2M9 12h4" /></>,
    dish: <><path d="M3 12a8 8 0 0 1 14-5" /><path d="M6 12a5 5 0 0 1 8-3" /><circle cx="10" cy="13" r="1.5" /><path d="M10 14.5L7 18M10 14.5L13 18" /></>,
    server: <><rect x="3" y="4" width="14" height="5" rx="1" /><rect x="3" y="11" width="14" height="5" rx="1" /><circle cx="6" cy="6.5" r="0.6" fill="currentColor" stroke="none" /><circle cx="6" cy="13.5" r="0.6" fill="currentColor" stroke="none" /></>,
    file: <><path d="M5 3h7l4 4v10a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1z" /><path d="M12 3v4h4" /></>,
    gear: <><circle cx="10" cy="10" r="2.5" /><path d="M10 2v2M10 16v2M2 10h2M16 10h2M4.2 4.2l1.4 1.4M14.4 14.4l1.4 1.4M4.2 15.8l1.4-1.4M14.4 5.6l1.4-1.4" /></>,
    search: <><circle cx="9" cy="9" r="5" /><path d="M13 13l3 3" /></>,
    chevronDown: <path d="M5 8l5 5 5-5" />,
    chevronRight: <path d="M8 5l5 5-5 5" />,
    plus: <path d="M10 4v12M4 10h12" />,
    warning: <><path d="M10 3l8 14H2L10 3z" /><path d="M10 8v4M10 14.5v.5" /></>,
    check: <path d="M4 10l4 4 8-8" />,
    x: <path d="M5 5l10 10M15 5L5 15" />,
    antenna: <><path d="M10 12l-3 5M10 12l3 5M10 12V4" /><path d="M6 7a5.6 5.6 0 0 1 8 0" /><path d="M7.5 9a3.5 3.5 0 0 1 5 0" /></>,
    radio: <><rect x="3" y="7" width="14" height="9" rx="1" /><path d="M6 4l4 3M14 4l-4 3" /><circle cx="13" cy="11.5" r="2" /><path d="M6 11h2M6 13h3" /></>,
    satellite: <><rect x="8" y="8" width="4" height="4" transform="rotate(45 10 10)" /><path d="M5 5l-2-2M15 15l2 2M15 5l2-2M5 15l-2 2" /></>,
    pin: <><path d="M10 2v6M7 8h6l-1 4H8l-1-4zM10 12v5" /></>,
    chart: <><path d="M3 15V5M3 15h14" /><path d="M6 12l3-4 3 2 4-5" /></>,
    play: <path d="M6 4l10 6-10 6V4z" />,
  };
  return (
    <svg width={size} height={size} viewBox="0 0 20 20" fill="none" stroke={color} strokeWidth={strokeWidth} strokeLinecap="round" strokeLinejoin="round" style={{ flexShrink: 0 }}>
      {paths[name]}
    </svg>
  );
}

export function StatusDot({ status = "nominal", size = 8, pulse = false }) {
  const colors = {
    nominal: T.green,
    warning: T.amber,
    error: T.red,
    info: T.gray,
    active: T.blue,
    idle: T.fgSubtle,
  };
  return (
    <span
      style={{
        display: "inline-block",
        width: size,
        height: size,
        borderRadius: "50%",
        background: colors[status] || T.gray,
        flexShrink: 0,
        animation: pulse ? "pulse-dot 1.6s ease-in-out infinite" : undefined,
      }}
    />
  );
}

export function SeverityBadge({ level }) {
  const palette = {
    INFO: { bg: T.grayTint, fg: "#5a5a5f" },
    WATCH: { bg: T.amberTint, fg: "#a85e00" },
    WARNING: { bg: T.amberTint, fg: "#a85e00" },
    WARNING_NEW: { bg: T.amberTint, fg: "#a85e00" },
    DISTRESS: { bg: T.redTint, fg: "#c7261d" },
    CRITICAL: { bg: T.redTint, fg: "#c7261d" },
    SEVERE: { bg: T.redTint, fg: "#c7261d" },
  };
  const colors = palette[level] || palette.INFO;
  return (
    <span
      style={{
        display: "inline-block",
        padding: "2px 8px",
        borderRadius: 999,
        background: colors.bg,
        color: colors.fg,
        fontFamily: T.fontMono,
        fontSize: 10.5,
        fontWeight: 600,
        letterSpacing: 0.3,
      }}
    >
      {level}
    </span>
  );
}

export function EnvBanner({ mode = "LIVE", text }) {
  const config = {
    LIVE: {
      bg: T.red,
      fg: "#fff",
      glyph: <span style={{ display: "inline-block", width: 8, height: 8, borderRadius: "50%", background: "#fff", animation: "pulse-bright 1.2s ease-in-out infinite" }} />,
      title: "LIVE YAMCS",
      sub: text || "Mission database connected",
    },
    HIL_SIM: {
      bg: T.amber,
      fg: "#231400",
      glyph: <span style={{ fontSize: 12 }}>◆</span>,
      title: "HIL SIMULATOR",
      sub: text || "Hardware in the loop",
    },
    SW_SIM: {
      bg: T.blue,
      fg: "#fff",
      glyph: <span style={{ fontSize: 12 }}>◇</span>,
      title: "SOFTWARE SIMULATOR",
      sub: text || "No hardware",
    },
  };
  const c = config[mode] || config.LIVE;
  return (
    <div
      style={{
        width: "100%",
        height: 36,
        background: c.bg,
        color: c.fg,
        display: "flex",
        alignItems: "center",
        justifyContent: "center",
        gap: 10,
        fontSize: 12.5,
        fontWeight: 600,
        letterSpacing: 0.3,
        flexShrink: 0,
      }}
    >
      <span style={{ display: "inline-flex", alignItems: "center", gap: 8 }}>
        {c.glyph}
        <span style={{ textTransform: "uppercase" }}>{c.title}</span>
      </span>
      <span style={{ opacity: 0.55, fontWeight: 500 }}>•</span>
      <span style={{ fontWeight: 500, opacity: 0.95 }}>{c.sub}</span>
    </div>
  );
}

export function Sidebar({ active, onNavigate }) {
  const items = [
    { id: "overview", label: "Overview", icon: "home" },
    { id: "telemetry", label: "Telemetry", icon: "activity" },
    { id: "commands", label: "Commands", icon: "terminal" },
    { id: "pass", label: "Pass Planning", icon: "dish" },
    { id: "ground", label: "Ground Services", icon: "server" },
  ];
  return (
    <div
      style={{
        width: 220,
        background: T.bgAlt,
        borderRight: `1px solid ${T.border}`,
        display: "flex",
        flexDirection: "column",
        flexShrink: 0,
      }}
    >
      <div style={{ padding: "20px 16px 16px", display: "flex", alignItems: "center", gap: 12 }}>
        <div
          style={{
            width: 36,
            height: 36,
            borderRadius: "50%",
            background: "#1d1d1f",
            display: "flex",
            alignItems: "center",
            justifyContent: "center",
            color: "#fff",
            fontFamily: T.fontMono,
            fontSize: 11,
            fontWeight: 700,
            letterSpacing: 0.5,
            border: `2px solid ${T.fg}`,
            boxShadow: `inset 0 0 0 2px ${T.bgAlt}`,
          }}
        >
          CS
        </div>
        <div>
          <div style={{ fontSize: 13, fontWeight: 600, color: T.fg, lineHeight: 1.2 }}>CubeSat</div>
          <div style={{ fontSize: 11, color: T.fgMuted, lineHeight: 1.2 }}>Ground Control</div>
        </div>
      </div>
      <div style={{ height: 1, background: T.border, margin: "0 16px" }} />
      <div style={{ padding: "12px 8px", flex: 1, display: "flex", flexDirection: "column" }}>
        {items.map((item) => {
          const isActive = item.id === active;
          return (
            <button
              key={item.id}
              onClick={() => onNavigate(item.id)}
              style={{
                appearance: "none",
                border: "none",
                background: isActive ? T.blueTint : "transparent",
                display: "flex",
                alignItems: "center",
                gap: 10,
                padding: "10px 12px",
                borderRadius: 8,
                color: isActive ? T.blue : T.fg,
                fontSize: 13,
                fontWeight: isActive ? 600 : 500,
                cursor: "pointer",
                textAlign: "left",
                marginBottom: 2,
              }}
            >
              <Icon name={item.icon} size={16} color={isActive ? T.blue : T.fgMuted} />
              <span>{item.label}</span>
            </button>
          );
        })}
        <div style={{ flex: 1 }} />
      </div>
    </div>
  );
}

export function Card({ children, padding = 20, style = {} }) {
  return (
    <div
      style={{
        background: T.bg,
        border: `1px solid ${T.border}`,
        borderRadius: 14,
        padding,
        ...style,
      }}
    >
      {children}
    </div>
  );
}

export function PageHeader({ title, subtitle, right }) {
  return (
    <div
      style={{
        padding: "16px 32px 14px",
        background: T.bg,
        borderBottom: `1px solid ${T.divider}`,
        display: "flex",
        alignItems: "center",
        justifyContent: "space-between",
        gap: 20,
      }}
    >
      <div>
        <div style={{ fontSize: 22, fontWeight: 600, color: T.fg, letterSpacing: -0.4 }}>{title}</div>
        <div style={{ fontSize: 12.5, color: T.fgMuted, marginTop: 3 }}>{subtitle}</div>
      </div>
      {right}
    </div>
  );
}

export function Sparkline({ points, color = T.green, width = 120, height = 32, fill = true }) {
  if (!points?.length) {
    return null;
  }
  const min = Math.min(...points);
  const max = Math.max(...points);
  const range = max - min || 1;
  const step = points.length > 1 ? width / (points.length - 1) : width;
  const coords = points.map((value, index) => [index * step, height - ((value - min) / range) * (height - 4) - 2]);
  const d = coords.map((point, index) => `${index === 0 ? "M" : "L"}${point[0]},${point[1]}`).join(" ");
  const areaPath = `${d} L${width},${height} L0,${height} Z`;
  return (
    <svg width={width} height={height} style={{ display: "block" }}>
      {fill ? <path d={areaPath} fill={color} opacity={0.1} /> : null}
      <path d={d} fill="none" stroke={color} strokeWidth={1.5} strokeLinecap="round" strokeLinejoin="round" />
    </svg>
  );
}

export function SubsysPill({ name }) {
  return (
    <span
      style={{
        display: "inline-flex",
        alignItems: "center",
        gap: 6,
        padding: "2px 8px",
        borderRadius: 999,
        background: T.grayTint,
        color: T.fgMuted,
        fontFamily: T.fontMono,
        fontSize: 10.5,
        fontWeight: 600,
        letterSpacing: 0.3,
      }}
    >
      {name}
    </span>
  );
}

export function EmptyState({ title, detail }) {
  return (
    <div style={{ display: "flex", flexDirection: "column", alignItems: "center", justifyContent: "center", height: "100%", color: T.fgMuted, padding: 32, textAlign: "center" }}>
      <div style={{ fontSize: 14, fontWeight: 600, color: T.fg, marginBottom: 6 }}>{title}</div>
      <div style={{ fontSize: 12.5, maxWidth: 440, lineHeight: 1.5 }}>{detail}</div>
    </div>
  );
}
