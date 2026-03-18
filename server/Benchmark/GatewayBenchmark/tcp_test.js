import ws from 'k6/ws';
import { check, sleep } from 'k6';

export const options = {
    stages: [
        { duration: '5s', target: 10 },  // 30秒内增加到 500 并发连接
        { duration: '5s', target: 10 },   // 保持 1 分钟
        { duration: '5s', target: 0 },    // 释放
    ],
};

const URL = 'ws://127.0.0.1:5005/test'; // 注意：指向你那个不需要鉴权的路径

export default function () {
    const res = ws.connect(URL, {}, function (socket) {
        socket.on('open', function () {
            // 连接成功后，每 10 秒发一个简单心跳，模拟长连接
            socket.setInterval(function () {
                socket.send('ping');
            }, 10000);
        });

        socket.on('error', function (e) {
            // console.error('WS Error: ', e.error());
        });

        // 每个连接保持 30 秒后主动断开，循环往复测试“建连/断连”能力
        socket.setTimeout(function () {
            socket.close();
        }, 30000);
    });

    check(res, {
        'status is 101': (r) => r && r.status === 101,
    });
}