function normalizeParts(qualifiedName) {
    return qualifiedName.split('/').filter(Boolean);
}

export function shortName(qualifiedName) {
    const parts = normalizeParts(qualifiedName);
    return parts[parts.length - 1] || qualifiedName;
}

export function subsystemPath(qualifiedName) {
    const parts = normalizeParts(qualifiedName);
    return parts.slice(1, -1);
}

export function subsystemLabel(qualifiedName) {
    const parts = subsystemPath(qualifiedName);
    if (!parts.length) {
        return 'Core';
    }
    return parts[parts.length - 1];
}

export function displaySubsystem(qualifiedName) {
    const parts = subsystemPath(qualifiedName);
    const focus = parts[0] || 'Core';
    return focus.replace(/([a-z])([A-Z])/g, '$1 $2').replace(/_/g, ' ').replace(/\b\w/g, (char) => char.toUpperCase());
}

export function valueFromYamcs(value) {
    if (!value) {
        return null;
    }
    switch (value.type) {
        case 'FLOAT':
            return value.floatValue;
        case 'DOUBLE':
            return value.doubleValue;
        case 'UINT32':
            return value.uint32Value;
        case 'SINT32':
            return value.sint32Value;
        case 'UINT64':
            return Number(value.uint64Value);
        case 'SINT64':
            return Number(value.sint64Value);
        case 'BOOLEAN':
            return value.booleanValue;
        case 'STRING':
            return value.stringValue;
        case 'TIMESTAMP':
            return value.stringValue || value.timestampValue;
        case 'ENUMERATED':
            return value.stringValue ?? Number(value.sint64Value);
        case 'ARRAY':
            return (value.arrayValue || []).map(valueFromYamcs);
        case 'AGGREGATE': {
            const names = value.aggregateValue?.name || [];
            const values = value.aggregateValue?.value || [];
            return Object.fromEntries(names.map((name, index) => [name, valueFromYamcs(values[index])]));
        }
        default:
            return value.stringValue ?? null;
    }
}

export function formatValue(raw, parameter) {
    if (raw == null) {
        return '—';
    }
    if (typeof raw === 'boolean') {
        return raw ? 'True' : 'False';
    }
    if (typeof raw === 'number' && Number.isFinite(raw)) {
        const digits = Math.abs(raw) >= 100 ? 0 : Math.abs(raw) >= 10 ? 1 : 2;
        return raw.toLocaleString(undefined, {
            maximumFractionDigits: digits,
            minimumFractionDigits: digits === 0 ? 0 : Math.min(1, digits),
        });
    }
    if (Array.isArray(raw)) {
        return raw.join(', ');
    }
    if (typeof raw === 'object') {
        return `${Object.keys(raw).length} fields`;
    }
    return String(raw);
}

export function unitFor(parameter) {
    return parameter?.type?.unitSet?.[0]?.unit || '';
}

export function isNumericParameter(parameter) {
    return ['integer', 'float'].includes(parameter?.type?.engType);
}

export function isAggregateParameter(parameter) {
    return parameter?.type?.engType === 'aggregate';
}

export function guessViz(parameter, latestValue) {
    const name = `${parameter?.qualifiedName || ''} ${parameter?.shortDescription || ''}`.toLowerCase();
    const unit = unitFor(parameter).toLowerCase();
    if (isAggregateParameter(parameter)) {
        return 'multibar';
    }
    if (!isNumericParameter(parameter)) {
        return 'detail';
    }
    if (unit === '%' || name.includes('usage')) {
        return 'bar';
    }
    if (unit === 'v' || name.includes('voltage')) {
        return 'radial';
    }
    if (unit.includes('deg') || name.includes('attitude') || name.includes('error') || name.includes('angle')) {
        return 'dial';
    }
    if (name.includes('rssi') || name.includes('snr') || name.includes('temp') || name.includes('power')) {
        return 'line';
    }
    if (typeof latestValue === 'number') {
        return 'line';
    }
    return 'detail';
}

function keywordScore(text, words) {
    return words.reduce((score, word) => score + (text.includes(word) ? 1 : 0), 0);
}

export function buildFeaturedParameterIds(parameters) {
    const candidates =
        parameters.filter((parameter) => isNumericParameter(parameter) || isAggregateParameter(parameter));
    const buckets = [
        ['battery', 'voltage', 'bus'],
        ['power', 'solar', 'current', 'watt'],
        ['temp', 'thermal', 'cpu'],
        ['rssi', 'snr', 'link', 'signal'],
        ['mode', 'state'],
        ['buffer', 'memory', 'storage'],
    ];
    const picks = [];
    for (const bucket of buckets) {
        const best = candidates
                         .map((parameter) => {
                             const haystack =
                                 `${parameter.qualifiedName} ${parameter.shortDescription || ''}`.toLowerCase();
                             return {parameter, score: keywordScore(haystack, bucket)};
                         })
                         .filter((item) => item.score > 0)
                         .sort((left, right) => right.score - left.score)[0]
                         ?.parameter;
        if (best && !picks.includes(best.qualifiedName)) {
            picks.push(best.qualifiedName);
        }
    }
    for (const parameter of candidates) {
        if (picks.length >= 8) {
            break;
        }
        if (!picks.includes(parameter.qualifiedName)) {
            picks.push(parameter.qualifiedName);
        }
    }
    return picks;
}

export function buildParameterTree(parameters) {
    const groups = new Map();
    for (const parameter of parameters) {
        const parts = subsystemPath(parameter.qualifiedName);
        const groupKey = parts[0] || 'Core';
        const label = displaySubsystem(parameter.qualifiedName);
        if (!groups.has(groupKey)) {
            groups.set(groupKey, {id: groupKey, label, items: []});
        }
        groups.get(groupKey).items.push(parameter);
    }
    return [...groups.values()]
        .map((group) => ({
                 ...group,
                 items: group.items.sort((left, right) => left.qualifiedName.localeCompare(right.qualifiedName)),
             }))
        .sort((left, right) => left.label.localeCompare(right.label));
}

export function buildCommandGroups(commands) {
    const groups = new Map();
    for (const command of commands) {
        const parts = subsystemPath(command.qualifiedName);
        const groupKey = parts[0] || 'Core';
        const label = displaySubsystem(command.qualifiedName);
        if (!groups.has(groupKey)) {
            groups.set(groupKey, {id: groupKey, label, items: []});
        }
        groups.get(groupKey).items.push(command);
    }
    return [...groups.values()]
        .map((group) => ({
                 ...group,
                 items: group.items.sort((left, right) => left.qualifiedName.localeCompare(right.qualifiedName)),
             }))
        .sort((left, right) => left.label.localeCompare(right.label));
}

export function latestById(parameterValues) {
    const result = new Map();
    for (const [qualifiedName, value] of Object.entries(parameterValues)) {
        result.set(qualifiedName, value);
    }
    return result;
}

export function buildParameterIdFromUpdate(update) {
    if (update.id?.namespace) {
        return `${update.id.namespace}/${update.id.name}`;
    }
    return update.id?.name;
}

export function severityTone(severity) {
    switch ((severity || '').toUpperCase()) {
        case 'SEVERE':
        case 'CRITICAL':
        case 'ERROR':
            return 'error';
        case 'WARNING':
        case 'WARNING_NEW':
        case 'DISTRESS':
        case 'WATCH':
            return 'warning';
        case 'INFO':
            return 'nominal';
        default:
            return 'idle';
    }
}

export function summarizeSubsystemHealth(parameters, valuesById) {
    const grouped = new Map();
    for (const parameter of parameters) {
        const key = displaySubsystem(parameter.qualifiedName);
        if (!grouped.has(key)) {
            grouped.set(key, {label: key, total: 0, warning: 0, stale: 0, nominal: 0});
        }
        const group = grouped.get(key);
        group.total += 1;
        const value = valuesById[parameter.qualifiedName];
        if (!value) {
            group.stale += 1;
            continue;
        }
        if (['WARNING', 'WARNING_NEW', 'DISTRESS', 'CRITICAL', 'SEVERE'].includes(value.monitoringResult)) {
            group.warning += 1;
        } else if (value.acquisitionStatus && value.acquisitionStatus !== 'ACQUIRED') {
            group.stale += 1;
        } else {
            group.nominal += 1;
        }
    }
    return [...grouped.values()].sort((left, right) => right.total - left.total).slice(0, 8);
}
