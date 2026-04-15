/* ============================================================
   sw.js — Smart Medicine Dispenser Service Worker v12
   Handles:  • Offline caching (cache-first strategy)
             • Web Push Notifications
   ============================================================ */

const CACHE_NAME = "med-app-v13";
const CACHED_URLS = [
  "./", 
  "./index.html",
  "./manifest.json",
  "./icon-192.png",
  "./icon-512.png"
];

// ── INSTALL: pre-cache shell ──────────────────────────────
self.addEventListener("install", e => {
  e.waitUntil(
    caches.open(CACHE_NAME).then(cache => cache.addAll(CACHED_URLS))
  );
  self.skipWaiting();
});

// ── ACTIVATE: clean old caches ────────────────────────────
self.addEventListener("activate", e => {
  e.waitUntil(
    caches.keys().then(keys =>
      Promise.all(
        keys.filter(k => k !== CACHE_NAME).map(k => caches.delete(k))
      )
    )
  );
  self.clients.claim();
});

// ── FETCH: cache-first for shell, network for API ────────
self.addEventListener("fetch", e => {
  const url = new URL(e.request.url);

  // Pass API calls (Blynk & Firebase) straight through (never cache)
  if (url.hostname.includes("blynk.cloud") || url.hostname.includes("firebaseio.com") || url.hostname.includes("googleapis.com")) {
    e.respondWith(fetch(e.request));
    return;
  }

  e.respondWith(
    caches.match(e.request).then(res => res || fetch(e.request))
  );
});

// ── PUSH: show native push notification ──────────────────
self.addEventListener("push", e => {
  let data = { title: "💊 Smart Medicine", body: "Update from your dispenser" };

  if (e.data) {
    try { data = e.data.json(); }
    catch { data.body = e.data.text(); }
  }

  const options = {
    body:    data.body,
    icon:    "https://cdn-icons-png.flaticon.com/512/3448/3448609.png",
    badge:   "https://cdn-icons-png.flaticon.com/512/3448/3448609.png",
    vibrate: [200, 100, 200],
    tag:     "medicine-alert",
    renotify: true,
    requireInteraction: data.body && data.body.toLowerCase().includes("miss"),
    actions: [
      { action: "ok",      title: "✔ OK"      },
      { action: "snooze",  title: "⏰ Snooze"  }
    ]
  };

  e.waitUntil(self.registration.showNotification(data.title || "💊 Smart Medicine", options));
});

// ── NOTIFICATION CLICK ────────────────────────────────────
self.addEventListener("notificationclick", e => {
  e.notification.close();
  if (e.action === "snooze") return;  // just dismiss

  e.waitUntil(
    clients.matchAll({ type: "window", includeUncontrolled: true }).then(list => {
      if (list.length) return list[0].focus();
      return clients.openWindow("./");
    })
  );
});