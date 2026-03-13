import React, { useState } from 'react';
import { User } from '../types';
import { MessageSquare, Mail, Lock, User as UserIcon, Loader2 } from 'lucide-react';
import CryptoJS from 'crypto-js';

// 密码加密辅助函数 (SHA-256)
async function hashPassword(password: string): Promise<string> {
  // 使用 crypto-js 替代原生 crypto.subtle，以兼容非 HTTPS (如局域网 IP) 环境
  return CryptoJS.SHA256(password).toString();
}

// 解析后端返回的错误信息
function parseErrorMessage(errorData: string, defaultMsg: string): string {
  if (!errorData) return defaultMsg;
  try {
    const json = JSON.parse(errorData);
    return json.message || errorData;
  } catch {
    return errorData;
  }
}

interface AuthProps {
  onLogin: (user: User) => void;
}

export function Auth({ onLogin }: AuthProps) {
  const [isLogin, setIsLogin] = useState(true);
  const [username, setUsername] = useState('');
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [isLoading, setIsLoading] = useState(false);
  const [errorMsg, setErrorMsg] = useState('');
  const [successMsg, setSuccessMsg] = useState('');

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setIsLoading(true);
    setErrorMsg('');
    setSuccessMsg('');

    try {
      // 在发送给后端前对密码进行 SHA-256 加密
      const hashedPassword = await hashPassword(password);

      if (isLogin) {
        // 调用 C++ 后端的登录接口
        const response = await fetch('/api/login', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: JSON.stringify({ email, password: hashedPassword }),
        });

        if (!response.ok) {
          const errorData = await response.text();
          throw new Error(parseErrorMessage(errorData, '登录失败，请检查邮箱和密码'));
        }

        // 登录成功
        onLogin({ username: email.split('@')[0], email });
      } else {
        // 调用 C++ 后端的注册接口
        const response = await fetch('/api/reg', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: JSON.stringify({ username, email, password: hashedPassword }),
        });

        if (!response.ok) {
          const errorData = await response.text();
          throw new Error(parseErrorMessage(errorData, '注册失败，请重试'));
        }

        // 注册成功后跳转到登录页面
        setSuccessMsg('注册成功，请登录！');
        setIsLogin(true);
        setPassword(''); // 清空密码框
      }
    } catch (err: any) {
      setErrorMsg(err.message);
    } finally {
      setIsLoading(false);
    }
  };

  return (
    <div className="min-h-screen w-full flex bg-[#0B0F19] selection:bg-indigo-500/30">
      {/* 左侧视觉展示区 (仅在桌面端显示) */}
      <div className="hidden lg:flex lg:w-1/2 relative overflow-hidden items-center justify-center border-r border-white/5">
        {/* 动态抽象背景 */}
        <div className="absolute inset-0 pointer-events-none">
          <div className="absolute top-[-20%] left-[-10%] w-[70%] h-[70%] bg-indigo-600/20 blur-[120px] rounded-full mix-blend-screen animate-pulse" style={{ animationDuration: '8s' }} />
          <div className="absolute bottom-[-20%] right-[-10%] w-[70%] h-[70%] bg-purple-600/20 blur-[120px] rounded-full mix-blend-screen animate-pulse" style={{ animationDuration: '10s', animationDelay: '2s' }} />
          {/* 网格纹理 */}
          <div className="absolute inset-0 bg-[linear-gradient(to_right,#ffffff05_1px,transparent_1px),linear-gradient(to_bottom,#ffffff05_1px,transparent_1px)] bg-[size:32px_32px]" />
        </div>

        {/* 左侧内容 */}
        <div className="relative z-10 p-12 text-white max-w-2xl">
          <div className="inline-flex items-center justify-center w-16 h-16 rounded-2xl bg-white/5 backdrop-blur-xl border border-white/10 mb-8 shadow-2xl">
            <MessageSquare size={32} className="text-indigo-400" />
          </div>
          <h1 className="text-5xl font-bold tracking-tight mb-6 leading-tight">
            连接每一个<br />
            <span className="text-transparent bg-clip-text bg-gradient-to-r from-indigo-400 to-purple-400">
              灵感瞬间
            </span>
          </h1>
          <p className="text-lg text-slate-400 leading-relaxed max-w-md">
            分布式消息系统为您提供极速、安全、智能的通讯体验。内置 Nano Banana 2 AI 引擎，让沟通突破文字的边界。
          </p>

          {/* 装饰性 UI 元素 */}
          <div className="mt-16 p-6 rounded-2xl bg-white/5 backdrop-blur-md border border-white/10 shadow-2xl w-80 transform -rotate-2 hover:rotate-0 transition-transform duration-500">
            <div className="flex items-center gap-4 mb-4">
              <div className="w-10 h-10 rounded-full bg-gradient-to-tr from-indigo-500 to-purple-500 flex items-center justify-center shadow-lg">
                <UserIcon size={20} className="text-white" />
              </div>
              <div>
                <div className="h-3 w-24 bg-white/20 rounded-full mb-2"></div>
                <div className="h-2 w-16 bg-white/10 rounded-full"></div>
              </div>
            </div>
            <div className="space-y-3">
              <div className="h-2 w-full bg-white/10 rounded-full"></div>
              <div className="h-2 w-4/5 bg-white/10 rounded-full"></div>
            </div>
          </div>
        </div>
      </div>

      {/* 右侧表单区 */}
      <div className="w-full lg:w-1/2 flex items-center justify-center p-8 sm:p-12 lg:p-24 relative">
        {/* 右侧动态背景装饰 */}
        <div className="absolute inset-0 overflow-hidden pointer-events-none">
          {/* 柔和的光晕 */}
          <div className="absolute top-[-10%] right-[-10%] w-[50%] h-[50%] bg-indigo-500/10 blur-[100px] rounded-full mix-blend-screen" />
          <div className="absolute bottom-[-10%] left-[-10%] w-[50%] h-[50%] bg-purple-500/10 blur-[100px] rounded-full mix-blend-screen" />
          {/* 极简的点阵背景 */}
          <div className="absolute inset-0 bg-[radial-gradient(#ffffff_1px,transparent_1px)] [background-size:24px_24px] opacity-[0.03]" />
        </div>

        <div className="w-full max-w-md relative z-10 bg-[#131825]/80 backdrop-blur-2xl p-8 sm:p-10 rounded-3xl shadow-[0_8px_40px_-12px_rgba(0,0,0,0.5)] border border-white/10 animate-in fade-in slide-in-from-bottom-4 duration-700">
          <div className="lg:hidden inline-flex items-center justify-center w-12 h-12 rounded-xl bg-white/5 mb-6 text-indigo-400 border border-white/10 shadow-sm">
            <MessageSquare size={24} />
          </div>

          <h2 className="text-3xl font-bold text-white mb-2 tracking-tight">
            {isLogin ? '欢迎回来' : '创建账号'}
          </h2>
          <p className="text-slate-400 mb-8">
            {isLogin ? '请输入您的邮箱和密码登录系统' : '填写以下信息开启您的智能通讯之旅'}
          </p>

          {errorMsg && (
            <div className="mb-6 p-4 bg-red-500/10 backdrop-blur-sm border border-red-500/20 text-red-400 rounded-xl text-sm flex items-center gap-2 shadow-sm">
              <div className="w-1.5 h-1.5 rounded-full bg-red-500 shrink-0" />
              {errorMsg}
            </div>
          )}

          {successMsg && (
            <div className="mb-6 p-4 bg-emerald-500/10 backdrop-blur-sm border border-emerald-500/20 text-emerald-400 rounded-xl text-sm flex items-center gap-2 shadow-sm">
              <div className="w-1.5 h-1.5 rounded-full bg-emerald-500 shrink-0" />
              {successMsg}
            </div>
          )}

          <form onSubmit={handleSubmit} className="space-y-5">
            {!isLogin && (
              <div className="space-y-1.5">
                <label className="block text-sm font-medium text-slate-300 ml-1">用户名</label>
                <div className="relative group">
                  <div className="absolute inset-y-0 left-0 pl-4 flex items-center pointer-events-none">
                    <UserIcon className="h-5 w-5 text-slate-500 group-focus-within:text-indigo-400 transition-colors" />
                  </div>
                  <input
                    type="text"
                    required
                    value={username}
                    onChange={(e) => setUsername(e.target.value)}
                    className="block w-full pl-12 pr-4 py-3.5 border border-white/10 rounded-2xl bg-black/20 hover:bg-black/40 focus:bg-black/40 focus:ring-2 focus:ring-indigo-500/50 focus:border-indigo-500/50 transition-all duration-300 text-white placeholder:text-slate-600 outline-none shadow-inner"
                    placeholder="请输入用户名"
                    disabled={isLoading}
                  />
                </div>
              </div>
            )}

            <div className="space-y-1.5">
              <label className="block text-sm font-medium text-slate-300 ml-1">邮箱</label>
              <div className="relative group">
                <div className="absolute inset-y-0 left-0 pl-4 flex items-center pointer-events-none">
                  <Mail className="h-5 w-5 text-slate-500 group-focus-within:text-indigo-400 transition-colors" />
                </div>
                <input
                  type="email"
                  required
                  value={email}
                  onChange={(e) => setEmail(e.target.value)}
                  className="block w-full pl-12 pr-4 py-3.5 border border-white/10 rounded-2xl bg-black/20 hover:bg-black/40 focus:bg-black/40 focus:ring-2 focus:ring-indigo-500/50 focus:border-indigo-500/50 transition-all duration-300 text-white placeholder:text-slate-600 outline-none shadow-inner"
                  placeholder="you@example.com"
                  disabled={isLoading}
                />
              </div>
            </div>

            <div className="space-y-1.5">
              <label className="block text-sm font-medium text-slate-300 ml-1">密码</label>
              <div className="relative group">
                <div className="absolute inset-y-0 left-0 pl-4 flex items-center pointer-events-none">
                  <Lock className="h-5 w-5 text-slate-500 group-focus-within:text-indigo-400 transition-colors" />
                </div>
                <input
                  type="password"
                  required
                  value={password}
                  onChange={(e) => setPassword(e.target.value)}
                  className="block w-full pl-12 pr-4 py-3.5 border border-white/10 rounded-2xl bg-black/20 hover:bg-black/40 focus:bg-black/40 focus:ring-2 focus:ring-indigo-500/50 focus:border-indigo-500/50 transition-all duration-300 text-white placeholder:text-slate-600 outline-none shadow-inner"
                  placeholder="••••••••"
                  disabled={isLoading}
                />
              </div>
            </div>

            <button
              type="submit"
              disabled={isLoading}
              className="w-full py-4 px-4 bg-indigo-600 hover:bg-indigo-500 text-white rounded-2xl font-medium shadow-lg shadow-indigo-900/20 transform hover:-translate-y-1 transition-all duration-300 disabled:opacity-70 disabled:cursor-not-allowed disabled:transform-none flex justify-center items-center mt-4 border border-indigo-500/50"
            >
              {isLoading ? (
                <Loader2 className="w-5 h-5 animate-spin" />
              ) : (
                isLogin ? '登录' : '注册账号'
              )}
            </button>
          </form>

          <div className="mt-8 text-center">
            <p className="text-slate-400 text-sm">
              {isLogin ? '还没有账号？' : '已经有账号了？'}{' '}
              <button
                onClick={() => {
                  setIsLogin(!isLogin);
                  setErrorMsg('');
                  setSuccessMsg('');
                }}
                className="text-indigo-400 font-semibold hover:text-indigo-300 hover:underline focus:outline-none transition-colors"
                disabled={isLoading}
              >
                {isLogin ? '立即注册' : '直接登录'}
              </button>
            </p>
          </div>
        </div>
      </div>
    </div>
  );
}
