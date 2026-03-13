/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

import React, { useState } from 'react';
import { ApiKeyPrompt } from './components/ApiKeyPrompt';
import { Auth } from './components/Auth';
import { Chat } from './components/Chat';
import { User } from './types';

export default function App() {
  const [hasKey, setHasKey] = useState(false);
  const [user, setUser] = useState<User | null>(null);

  if (!hasKey) {
    return <ApiKeyPrompt onKeySelected={() => setHasKey(true)} />;
  }

  if (!user) {
    return <Auth onLogin={setUser} />;
  }

  return <Chat user={user} onLogout={() => setUser(null)} />;
}
