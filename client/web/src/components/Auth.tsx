import React, { useState } from 'react';
import { User } from '../types';
import { Loader2, X as XIcon } from 'lucide-react';
import CryptoJS from 'crypto-js';

// 密码加密辅助函数 (SHA-256)
async function hashPassword(password: string): Promise<string> {
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
      const hashedPassword = await hashPassword(password);
      const endpoint = isLogin ? '/api/login' : '/api/reg';
      
      const res = await fetch(endpoint, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(isLogin ? { email, password: hashedPassword } : { username, email, password: hashedPassword }),
      });

      if (!res.ok) {
        const errorData = await res.text();
        throw new Error(parseErrorMessage(errorData, isLogin ? '登录失败，请检查邮箱和密码' : '注册失败，请重试'));
      }

      if (isLogin) {
          let userinfo = { username: email.split('@')[0], email };
          try {
            const respData = await res.json();
            if (respData && respData.userinfo) {
              userinfo = { ...userinfo, ...respData.userinfo };
            }
          } catch (e) {}
          onLogin(userinfo);
      } else {
          setSuccessMsg('注册成功，请登录！');
          setIsLogin(true);
          setPassword('');
      }
    } catch (err: any) {
      setErrorMsg(err.message);
    } finally {
      setIsLoading(false);
    }
  };

  return (
    <div className="min-h-screen w-full flex flex-col items-center justify-center bg-black text-white font-sans selection:bg-zinc-800">
      <div className="w-full max-w-md p-8 flex flex-col items-center">
        {/* Logo Area */}
        <div className="mb-10">
          <XIcon size={48} className="text-white" strokeWidth={1.5} />
        </div>

        <h1 className="text-3xl font-bold mb-8 w-full text-left">
          {isLogin ? 'Sign in to XChat' : 'Join XChat today'}
        </h1>

        {errorMsg && (
          <div className="w-full mb-6 p-3 bg-red-500/10 border border-red-500/20 text-red-500 rounded-md text-sm">
            {errorMsg}
          </div>
        )}

        {successMsg && (
          <div className="w-full mb-6 p-3 bg-emerald-500/10 border border-emerald-500/20 text-emerald-500 rounded-md text-sm">
            {successMsg}
          </div>
        )}

        <form onSubmit={handleSubmit} className="w-full space-y-5">
          {!isLogin && (
            <div>
              <input
                type="text"
                required
                value={username}
                onChange={(e) => setUsername(e.target.value)}
                className="w-full px-4 py-4 bg-black border border-zinc-800 rounded-md focus:outline-none focus:border-zinc-600 focus:ring-1 focus:ring-zinc-600 transition-colors text-white placeholder-zinc-500 text-lg"
                placeholder="Username"
                disabled={isLoading}
              />
            </div>
          )}

          <div>
            <input
              type="email"
              required
              value={email}
              onChange={(e) => setEmail(e.target.value)}
              className="w-full px-4 py-4 bg-black border border-zinc-800 rounded-md focus:outline-none focus:border-zinc-600 focus:ring-1 focus:ring-zinc-600 transition-colors text-white placeholder-zinc-500 text-lg"
              placeholder="Email"
              disabled={isLoading}
            />
          </div>

          <div>
            <input
              type="password"
              required
              value={password}
              onChange={(e) => setPassword(e.target.value)}
              className="w-full px-4 py-4 bg-black border border-zinc-800 rounded-md focus:outline-none focus:border-zinc-600 focus:ring-1 focus:ring-zinc-600 transition-colors text-white placeholder-zinc-500 text-lg"
              placeholder="Password"
              disabled={isLoading}
            />
          </div>

          <button
            type="submit"
            disabled={isLoading}
            className="w-full py-4 bg-white text-black rounded-full font-bold text-lg hover:bg-zinc-200 transition-colors disabled:opacity-50 disabled:cursor-not-allowed flex justify-center items-center mt-4"
          >
            {isLoading ? (
              <Loader2 className="w-6 h-6 animate-spin" />
            ) : (
              isLogin ? 'Sign in' : 'Sign up'
            )}
          </button>
        </form>

        <div className="mt-8 w-full text-left">
          <span className="text-zinc-500">
            {isLogin ? "Don't have an account?" : "Already have an account?"}
          </span>{' '}
          <button
            onClick={() => {
              setIsLogin(!isLogin);
              setErrorMsg('');
              setSuccessMsg('');
            }}
            className="text-blue-500 hover:underline focus:outline-none"
            disabled={isLoading}
          >
            {isLogin ? 'Sign up' : 'Sign in'}
          </button>
        </div>
      </div>
    </div>
  );
}
