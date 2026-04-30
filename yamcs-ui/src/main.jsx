import React from "react";
import ReactDOM from "react-dom/client";
import "./index.css";
import { discoverContext, getParameterHistory, issueCommand, listCommandHistory, listCommands, listEvents, listLinks, listParameters, YamcsWebSocket } from "./lib/yamcs";
import { buildCommandGroups, buildFeaturedParameterIds, buildParameterIdFromUpdate, buildParameterTree, valueFromYamcs } from "./lib/model";
import { OverviewScreen } from "./screens/OverviewScreen";
import { TelemetryScreen } from "./screens/TelemetryScreen";
import { CommandsScreen } from "./screens/CommandsScreen";
import { PassPlanningScreen } from "./screens/PassPlanningScreen";
import { GroundServicesScreen } from "./screens/GroundServicesScreen";

function useRoute() {
  const initial = window.location.hash.replace(/^#/, "") || "overview";
  const [route, setRoute] = React.useState(initial);
  React.useEffect(() => {
    const onHashChange = () => setRoute(window.location.hash.replace(/^#/, "") || "overview");
    window.addEventListener("hashchange", onHashChange);
    return () => window.removeEventListener("hashchange", onHashChange);
  }, []);
  const navigate = React.useCallback((next) => {
    window.location.hash = next;
  }, []);
  return [route, navigate];
}

function App() {
  const [route, navigate] = useRoute();
  const [context, setContext] = React.useState(null);
  const [parameters, setParameters] = React.useState([]);
  const [commands, setCommands] = React.useState([]);
  const [events, setEvents] = React.useState([]);
  const [commandHistory, setCommandHistory] = React.useState([]);
  const [links, setLinks] = React.useState([]);
  const [parameterValues, setParameterValues] = React.useState({});
  const [history, setHistory] = React.useState([]);
  const [historyLoading, setHistoryLoading] = React.useState(false);
  const [currentTime, setCurrentTime] = React.useState(null);
  const [selectedParameterId, setSelectedParameterId] = React.useState(null);
  const [error, setError] = React.useState("");
  const [issuingCommand, setIssuingCommand] = React.useState(false);
  const socketRef = React.useRef(null);
  const numericMapRef = React.useRef({});

  React.useEffect(() => {
    let cancelled = false;
    (async () => {
      try {
        const discovered = await discoverContext();
        const [parameterList, commandList, eventList, commandListHistory, linkList] = await Promise.all([
          listParameters(discovered.instance),
          listCommands(discovered.instance),
          listEvents(discovered.instance),
          listCommandHistory(discovered.instance),
          listLinks(discovered.instance),
        ]);
        if (cancelled) {
          return;
        }
        setContext(discovered);
        setParameters(parameterList);
        setCommands(commandList);
        setEvents(eventList);
        setCommandHistory(commandListHistory);
        setLinks(linkList);
        const defaults = buildFeaturedParameterIds(parameterList);
        setSelectedParameterId(defaults[0] || parameterList[0]?.qualifiedName || null);
      } catch (caughtError) {
        if (!cancelled) {
          setError(caughtError.message);
        }
      }
    })();
    return () => {
      cancelled = true;
    };
  }, []);

  React.useEffect(() => {
    if (!context || !parameters.length) {
      return undefined;
    }
    const socket = new YamcsWebSocket();
    socketRef.current = socket;
    const ids = parameters.map((parameter) => ({ name: parameter.qualifiedName }));
    (async () => {
      try {
        await socket.subscribe(
          "parameters",
          {
            instance: context.instance,
            processor: context.processor,
            id: ids,
            abortOnInvalid: false,
            sendFromCache: true,
            updateOnExpiration: true,
            action: "REPLACE",
          },
          (data) => {
            if (data.mapping) {
              numericMapRef.current = {
                ...numericMapRef.current,
                ...Object.fromEntries(
                  Object.entries(data.mapping).map(([numericId, namedObject]) => [
                    numericId,
                    namedObject.namespace ? `${namedObject.namespace}/${namedObject.name}` : namedObject.name,
                  ]),
                ),
              };
            }
            if (!data.values?.length) {
              return;
            }
            setParameterValues((current) => {
              const next = { ...current };
              for (const update of data.values) {
                const qualifiedName = update.numericId != null ? numericMapRef.current[update.numericId] : buildParameterIdFromUpdate(update);
                if (qualifiedName) {
                  next[qualifiedName] = {
                    ...update,
                    parsedValue: valueFromYamcs(update.engValue || update.rawValue),
                  };
                }
              }
              return next;
            });
          },
        );
        await socket.subscribe("events", { instance: context.instance }, (data) => {
          setEvents((current) => [data, ...current].slice(0, 20));
        });
        await socket.subscribe(
          "commands",
          { instance: context.instance, processor: context.processor, ignorePastCommands: true },
          (data) => {
            setCommandHistory((current) => [data, ...current].slice(0, 20));
          },
        );
        await socket.subscribe("links", { instance: context.instance }, (data) => {
          setLinks(data.links || []);
        });
        await socket.subscribe("time", { instance: context.instance, processor: context.processor }, (data) => {
          setCurrentTime(data.value || data.stringValue || data);
        });
      } catch (caughtError) {
        setError(caughtError.message);
      }
    })();
    return () => socket.close();
  }, [context, parameters]);

  React.useEffect(() => {
    if (!context || !selectedParameterId) {
      return;
    }
    const selectedParameter = parameters.find((parameter) => parameter.qualifiedName === selectedParameterId);
    if (!selectedParameter || !["integer", "float"].includes(selectedParameter.type?.engType)) {
      setHistory([]);
      return;
    }
    let cancelled = false;
    setHistoryLoading(true);
    getParameterHistory(context.instance, selectedParameterId, 80)
      .then((values) => {
        if (cancelled) {
          return;
        }
        setHistory(
          values
            .map((entry) => ({
              generationTime: entry.generationTime,
              value: valueFromYamcs(entry.engValue || entry.rawValue),
            }))
            .reverse(),
        );
      })
      .catch((caughtError) => {
        if (!cancelled) {
          setError(caughtError.message);
        }
      })
      .finally(() => {
        if (!cancelled) {
          setHistoryLoading(false);
        }
      });
    return () => {
      cancelled = true;
    };
  }, [context, parameters, selectedParameterId]);

  const parameterById = React.useMemo(
    () => Object.fromEntries(parameters.map((parameter) => [parameter.qualifiedName, parameter])),
    [parameters],
  );
  const commandById = React.useMemo(
    () => Object.fromEntries(commands.map((command) => [command.qualifiedName, command])),
    [commands],
  );
  const parameterTree = React.useMemo(() => buildParameterTree(parameters), [parameters]);
  const commandGroups = React.useMemo(() => buildCommandGroups(commands), [commands]);
  const featuredParameterIds = React.useMemo(() => buildFeaturedParameterIds(parameters), [parameters]);

  const sharedProps = {
    context,
    error,
    navigate,
    route,
    parameters,
    parameterTree,
    parameterById,
    parameterValues,
    links,
    events,
    commands,
    commandGroups,
    commandById,
    commandHistory,
    currentTime,
    selectedParameterId,
    setSelectedParameterId,
    featuredParameterIds,
    history,
    historyLoading,
    issueCommand: async (qualifiedName, args) => {
      if (!context) {
        return;
      }
      setIssuingCommand(true);
      try {
        const response = await issueCommand(context.instance, context.processor, qualifiedName, args);
        setCommandHistory((current) => [response, ...current].slice(0, 20));
        return response;
      } finally {
        setIssuingCommand(false);
      }
    },
    issuingCommand,
  };

  if (route === "telemetry") {
    return <TelemetryScreen {...sharedProps} />;
  }
  if (route === "commands") {
    return <CommandsScreen {...sharedProps} />;
  }
  if (route === "pass") {
    return <PassPlanningScreen {...sharedProps} />;
  }
  if (route === "ground") {
    return <GroundServicesScreen {...sharedProps} />;
  }
  return <OverviewScreen {...sharedProps} />;
}

ReactDOM.createRoot(document.getElementById("root")).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>,
);
