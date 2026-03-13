import React, { useState, useEffect } from 'react';
import { Key } from 'lucide-react';

export function ApiKeyPrompt({ onKeySelected }: { onKeySelected: () => void }) {
  const [isChecking, setIsChecking] = useState(true);

  useEffect(() => {
    const checkKey = async () => {
      try {
        // @ts-ignore
        const hasKey = await window.aistudio?.hasSelectedApiKey();
        if (hasKey) {
          onKeySelected();
        }
      } catch (e) {
        console.error(e);
      } finally {
        setIsChecking(false);
      }
    };
    checkKey();
  }, [onKeySelected]);

  const handleSelectKey = async () => {
    try {
      // @ts-ignore
      await window.aistudio?.openSelectKey();
      onKeySelected();
    } catch (e) {
      console.error(e);
    }
  };

  if (isChecking) return null;

  return (
    <div className="min-h-screen flex items-center justify-center bg-gradient-to-br from-indigo-500 to-purple-600 p-4">
      <div className="bg-white rounded-2xl shadow-2xl p-8 max-w-md w-full text-center">
        <div className="w-16 h-16 bg-indigo-100 rounded-full flex items-center justify-center mx-auto mb-6">
          <Key className="w-8 h-8 text-indigo-600" />
        </div>
        <h2 className="text-2xl font-bold text-gray-800 mb-4">需要 API Key</h2>
        <p className="text-gray-600 mb-6 text-sm">
          为了使用高级的 AI 图片生成功能 (Nano Banana 2)，请选择您的 Google Cloud API Key。
          <br/><br/>
          <a href="https://ai.google.dev/gemini-api/docs/billing" target="_blank" rel="noreferrer" className="text-indigo-600 hover:underline">
            了解有关计费的更多信息
          </a>
        </p>
        <button
          onClick={handleSelectKey}
          className="w-full py-3 px-4 bg-gradient-to-r from-indigo-600 to-purple-600 text-white rounded-xl font-medium hover:shadow-lg transform hover:-translate-y-0.5 transition-all"
        >
          选择 API Key
        </button>
      </div>
    </div>
  );
}
