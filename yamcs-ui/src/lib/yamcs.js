const jsonHeaders = {
    Accept: 'application/json',
    'Content-Type': 'application/json',
};

export async function fetchJson(path, init = {}) {
    const response = await fetch(path, {
        ...init,
        headers: {
            ...jsonHeaders,
            ...(init.headers || {}),
        },
    });
    if (!response.ok) {
        let detail = `HTTP ${response.status}`;
        try {
            const payload = await response.json();
            detail = payload?.exception?.msg || payload?.exception?.type || detail;
        } catch {
            // Ignore parse errors on non-json error bodies.
        }
        throw new Error(detail);
    }
    return response.json();
}

export async function discoverContext() {
    const payload = await fetchJson('/api/instances');
    const instance = payload.instances?.[0];
    const processor = instance?.processors?.find((item) => item.state === 'RUNNING') || instance?.processors?.[0];
    if (!instance || !processor) {
        throw new Error('No Yamcs instance or processor is available.');
    }
    return {
        instance: instance.name,
        processor: processor.name,
        serverTag: instance.name,
        missionTime: instance.missionTime,
    };
}

export async function listParameters(instance) {
    const payload = await fetchJson(`/api/mdb/${encodeURIComponent(instance)}/parameters?details=true&limit=500`);
    return payload.parameters || [];
}

export async function listCommands(instance) {
    const payload = await fetchJson(`/api/mdb/${encodeURIComponent(instance)}/commands?details=true&limit=500`);
    return payload.commands || [];
}

export async function listEvents(instance, limit = 20) {
    const payload = await fetchJson(`/api/archive/${encodeURIComponent(instance)}/events?limit=${limit}`);
    return payload.events || payload.event || [];
}

export async function listCommandHistory(instance, limit = 20) {
    const payload = await fetchJson(`/api/archive/${encodeURIComponent(instance)}/commands?limit=${limit}`);
    return payload.entry || [];
}

export async function listLinks(instance) {
    const payload = await fetchJson(`/api/links/${encodeURIComponent(instance)}`);
    return payload.links || [];
}

export async function getParameterHistory(instance, qualifiedName, limit = 60) {
    const payload = await fetchJson(
        `/api/archive/${encodeURIComponent(instance)}/parameters/${encodeURIComponent(qualifiedName)}?limit=${limit}`,
    );
    return payload.parameter || [];
}

export async function issueCommand(instance, processor, qualifiedName, args = {}) {
    return fetchJson(
        `/api/processors/${encodeURIComponent(instance)}/${encodeURIComponent(processor)}/commands/${
            encodeURIComponent(qualifiedName)}`,
        {
            method: 'POST',
            body: JSON.stringify({
                origin: 'cubeground-ui',
                sequenceNumber: Date.now() % 2147483647,
                args,
            }),
        },
    );
}

export class YamcsWebSocket {
    constructor() {
        this.ws = null;
        this.messageId = 1;
        this.pendingReplies = new Map();
        this.subscriptions = new Map();
        this.callMap = new Map();
        this.listeners = new Set();
    }

    connect() {
        if (this.ws && this.ws.readyState <= 1) {
            return;
        }
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        this.ws = new WebSocket(`${protocol}//${window.location.host}/api/websocket`);
        this.ws.addEventListener('message', (event) => {
            const message = JSON.parse(event.data);
            if (message.type === 'reply') {
                const replyId = message.data?.reply_to;
                const resolver = this.pendingReplies.get(replyId);
                if (resolver) {
                    this.pendingReplies.delete(replyId);
                    if (message.data?.exception) {
                        resolver.reject(new Error(message.data.exception.msg || message.data.exception.type));
                    } else {
                        resolver.resolve(message.call);
                    }
                }
                return;
            }
            const subscription = message.call != null ? this.callMap.get(message.call) : null;
            if (subscription?.onData) {
                subscription.onData(message.data, message);
            }
            for (const listener of this.listeners) {
                listener(message);
            }
        });
    }

    waitForOpen() {
        this.connect();
        if (this.ws.readyState === WebSocket.OPEN) {
            return Promise.resolve();
        }
        return new Promise((resolve, reject) => {
            const cleanup = () => {
                this.ws.removeEventListener('open', onOpen);
                this.ws.removeEventListener('error', onError);
            };
            const onOpen = () => {
                cleanup();
                resolve();
            };
            const onError = () => {
                cleanup();
                reject(new Error('WebSocket connection failed'));
            };
            this.ws.addEventListener('open', onOpen);
            this.ws.addEventListener('error', onError);
        });
    }

    async subscribe(type, options, onData) {
        await this.waitForOpen();
        const id = this.messageId++;
        const call = await new Promise((resolve, reject) => {
            this.pendingReplies.set(id, {resolve, reject});
            this.ws.send(JSON.stringify({type, id, options}));
        });
        this.callMap.set(call, {type, options, onData});
        return call;
    }

    sendStream(type, call, options) {
        if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
            return;
        }
        this.ws.send(JSON.stringify({type, call, options}));
    }

    close() {
        if (this.ws) {
            this.ws.close();
        }
    }
}
