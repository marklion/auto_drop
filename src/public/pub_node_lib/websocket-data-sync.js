class DataSyncServer {
    constructor(options) {
        const WebSocket = require('ws');
        this.wss = new WebSocket.Server(options);
        this._dataStore = new Map();
        this._listeners = new Map();

        this.wss.on('connection', (ws) => {
            // 同步初始数据
            ws.send(JSON.stringify({ type: 'init', data: Object.fromEntries(this._dataStore) }));

            // 接收客户端消息
            ws.on('message', (message) => {
                const { type, key, value } = JSON.parse(message);
                if (type === 'set') this._handleSet(key, value, ws);
            });
        });
    }

    _handleSet(key, value, sourceWs) {
        this._dataStore.set(key, value);

        // 广播给所有客户端（排除来源）
        this.wss.clients.forEach(client => {
            if (client !== sourceWs && client.readyState === WebSocket.OPEN) {
                client.send(JSON.stringify({ type: 'update', key, value }));
            }
        });

        // 触发本地监听
        if (this._listeners.has(key)) {
            this._listeners.get(key).forEach(cb => cb(value));
        }
    }

    setData(key, value) {
        this._dataStore.set(key, value);
        this.wss.clients.forEach(client => {
            client.send(JSON.stringify({ type: 'update', key, value }));
        });
    }

    watchData(callback) {
        this._listeners.set('global', [...(this._listeners.get('global') || []), callback]);
    }
}
class DataSyncClient {
    constructor(url) {
        this.ws = new WebSocket(url);
        this._dataStore = new Map();
        this._listeners = new Map();

        this.ws.onmessage = (message) => {
            const { type, key, value, data } = JSON.parse(message.data);
            if (type === 'init') {
                Object.entries(data).forEach(([k, v]) => this._dataStore.set(k, v));
            } else if (type === 'update') {
                this._dataStore.set(key, value);
                this._triggerListeners(key, value);
            }
        };
    }

    _triggerListeners(key, value) {
        const globalListeners = this._listeners.get('global') || [];
        const keyListeners = this._listeners.get(key) || [];
        [...globalListeners, ...keyListeners].forEach(cb => cb(key, JSON.parse(value)));
    }

    setData(key, value) {
        this._dataStore.set(key, value);
        this.ws.send(JSON.stringify({ type: 'set', key, value }));
    }

    watchData(callback, specificKey) {
        const key = specificKey || 'global';
        this._listeners.set(key, [...(this._listeners.get(key) || []), callback]);
    }
}
module.exports = { DataSyncServer, DataSyncClient };