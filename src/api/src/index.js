const express = require('express');
const app = express();
const Redis = require('ioredis');
const fs = require('fs');
const yaml = require('js-yaml');

app.use(express.json()) // for parsing application/json
app.use(express.urlencoded({ extended: true })) // for parsing application/x-www-form-urlencoded

async function get_redis_config() {
    const configPath = '/conf/config.yaml';
    try {
        const fileContents = fs.readFileSync(configPath, 'utf8');
        const data = yaml.load(fileContents);
        return data.redis;
    } catch (e) {
        console.log(e);
        return null;
    }
}

app.post('/api/redis/set', async (req, res) => {
    let ret = {
        err_msg: '无权限'
    };
    let redis_config = await get_redis_config();
    if (redis_config) {
        const redis = new Redis({
            host: redis_config.host,
            port: redis_config.port,
            password: redis_config.password
        })
        await redis.hset("appliance:" + process.env.SELF_SERIAL, req.body.key, req.body.value);
        redis.quit();
        ret = {
            err_msg: ""
        };
    }

    res.send(ret);
});

app.post('/api/redis/get', async (req, res) => {
    let ret = {
        err_msg: '无权限',
    };
    let redis_config = await get_redis_config();
    if (redis_config) {
        const redis = new Redis({
            host: redis_config.host,
            port: redis_config.port,
            password: redis_config.password
        })
        ret.result = {
            value: await redis.hget("appliance:" + process.env.SELF_SERIAL, req.body.key),
        };
        redis.quit();
        ret.err_msg = "";
    }

    res.send(ret);
});

app.post('/api/redis/pub', async (req, res) => {
    let ret = {
        err_msg: '无权限'
    };
    let redis_config = await get_redis_config();
    if (redis_config) {
        const redis = new Redis({
            host: redis_config.host,
            port: redis_config.port,
            password: redis_config.password
        })
        let pub_req = req.body.pub_req;
        let pub_path = req.body.pub_path;
        await redis.publish(pub_path + process.env.SELF_SERIAL, pub_req);
        redis.quit();
        ret = {
            err_msg: ""
        };
    }

    res.send(ret);
});

process.on('uncaughtException', (err) => {
    console.error('An uncaught error occurred!');
    console.error(err.stack);
});
let server = app.listen(23311, () => console.log('Server running on port 23311'));
process.on('SIGINT', () => {
    console.log('SIGINT signal received. Closing server...');
    server.close();
});