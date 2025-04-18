const express = require('express');
const app = express();
const Redis = require('ioredis');
const fs = require('fs');
const yaml = require('js-yaml');
const { DataSyncServer } = require('../../public/pub_node_lib/websocket-data-sync.js');

app.use(express.json()) // for parsing application/json
app.use(express.urlencoded({ extended: true })) // for parsing application/x-www-form-urlencoded
const ws_server = new DataSyncServer({ port: 23312 });

app.post('/api/update', async (req, res) => {
    const util = require('node:util');
    const exec = util.promisify(require('node:child_process').exec);
    await exec(`cp ${req.file_path} /root/install.sh`);
    await exec('chmod +x /root/install.sh');
    res.send({ err_msg: '' });
    await exec("kill -9 $(ps ax | grep core_daemon | grep -v grep | awk '{print $1}')");
});

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
        await redis.publish(pub_path + process.env.SELF_SERIAL, JSON.stringify(pub_req));
        redis.quit();
        ret = {
            err_msg: ""
        };
    }

    res.send(ret);
});
app.post('/api/update_state', async (req, res) => {
    let redis_config = await get_redis_config();
    if (redis_config) {
        const redis = new Redis({
            host: redis_config.host,
            port: redis_config.port,
            password: redis_config.password
        })
        let state = await redis.hget("appliance:" + process.env.SELF_SERIAL, "current_state_");
        redis.quit();
        ws_server.setData('current_state_', state);
    }
    res.send({ err_msg: '' });
})
const multer = require('multer');
const upload = multer({ dest: '/database/uploads/' });
app.post('/api/upload', upload.single('file'), (req, res) => {
    const path = require('path');
    const fs = require('fs');
    let fileExtension = path.extname(req.file.originalname);
    fs.readFile(req.file.path, (err, data) => {
        if (err) {
            console.error(err);
            res.send({ err_msg: '文件读取失败' });
            return;
        }
        let base64Content = data.toString('base64');
        const decodedData = Buffer.from(base64Content, 'base64');
        const uuid = require('uuid');
        real_file_name = uuid.v4();
        const filePath = '/uploads/' + real_file_name + fileExtension;
        fs.writeFileSync('/database' + filePath, decodedData);
        res.send(filePath);
    });
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
