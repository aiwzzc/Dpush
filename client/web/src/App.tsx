/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

import React, { useState } from 'react';
import { Auth } from './components/Auth';
import { Chat } from './components/Chat';
import { User } from './types';

export default function App() {
  const [user, setUser] = useState<User | null>(null);

  if (!user) {
    return <Auth onLogin={setUser} />;
  }

  return <Chat user={user} onLogout={() => setUser(null)} />;
}
