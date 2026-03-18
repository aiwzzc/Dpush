import http from 'k6/http';
import ws from 'k6/ws';
import { check, sleep } from 'k6';

// export const options = {
//     stages: [
//         { duration: '30s', target: 500 }, // 提升到 500 并发
//         { duration: '1m', target: 500 },
//         { duration: '30s', target: 0 },
//     ],
// };

export const options = {
    vus: 50, // 只用 50 个并发长连接
    duration: '30s',
};

const GATEWAY_HTTP_URL = 'http://127.0.0.1:5005/api/login';
const GATEWAY_WS_URL = 'ws://127.0.0.1:5005/ws';

export default function () {
    // 1. 匹配你数据库的随机用户逻辑 (假设 ID 1-1000000)
    const randomId = Math.floor(Math.random() * 1000000) + 1;
    const email = `user_${randomId}@test.com`;

    // 2. 发送登录请求
    const payload = JSON.stringify({
        email: email,
        password: '123456' // 确保这里发送的是明文，由 C++ 后端去做 MD5(123456 + salt)
    });

    const loginRes = http.post(GATEWAY_HTTP_URL, payload, {
        headers: { 'Content-Type': 'application/json' },
    });

    // 提取 Cookie 中的 sid
    const sid = loginRes.cookies.sid ? loginRes.cookies.sid[0].value : null;

    // 校验登录
    // const loginOk = check(loginRes, {
    //     'login success (200)': (r) => r.status === 200,
    //     'has sid cookie': () => sid !== null,
    // });

    // if (!loginOk) {
    //     // 如果登录失败，记录一下原因并退出当前循环
    //     // console.error(`Login failed for ${email}: ${loginRes.body}`);
    //     sleep(1);
    //     return;
    // }

    // 3. 建立 WebSocket 连接
    const wsRes = ws.connect(GATEWAY_WS_URL, { headers: { 'Cookie': `sid=${sid}` } }, function (socket) {
        // socket.on('open', () => {
        //     // // 每 5 秒心跳
        //     // socket.setInterval(() => {
        //     //     socket.send(JSON.stringify({ cmd: 'heartbeat' }));
        //     // }, 5000);
        //     // 握手成功后直接发一个包就关闭，测试瞬时处理能力
        //     socket.send(JSON.stringify({ cmd: 'hello' }));
        //     // sleep(0.01);
        //     socket.close(); // 立即关闭
        // });

        // socket.on('error', (e) => console.error(`WS Error: ${e.error()}`));

        // // 每个用户保持连接 30 秒，模拟真实在线
        // socket.setTimeout(() => socket.close(), 30000);
        socket.on('open', () => {
            // 连上之后，死循环或者高频发包，不断开连接
            socket.setInterval(() => {
                socket.send(JSON.stringify({ cmd: 'hello' }));
            }, 10); // 每 10 毫秒发一次，50个连接一秒钟就是 5000 个包
        });

        // 取消 socket.close() 的逻辑
        socket.setTimeout(() => { socket.close(); }, 30000); 
    });

    // check(wsRes, { 'ws handshake 101': (r) => r && r.status === 101 });
}