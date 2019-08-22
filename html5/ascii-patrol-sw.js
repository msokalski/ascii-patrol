var CACHE_NAME = 'ascii-patrol-cache-v1.5';
var urlsToCache = [
  'temp.js',
  'temp.html.mem',
  '8x14x16.png',
  'ascii-patrol-html5.html',
  'ascii-patrol-html5.json',
  'ascii-patrol-html5.png'
];

self.addEventListener('install', function (event) {
	// Perform install steps
	event.waitUntil(
    caches.open(CACHE_NAME)
      .then(function (cache) {
      	console.log('Opened cache');
      	return cache.addAll(urlsToCache);
      })
  );
});

self.addEventListener('fetch', function (event) {
  event.respondWith(
    caches.match(event.request)
      .then(function (response) {
        // Cache hit - return response
        if (response) {
      	  return response;
        }

      return fetch(event.request).then(
        function (response) {
          // Check if we received a valid response
          if (!response || response.status !== 200 || response.type !== 'basic') {
          	return response;
          }

          // IMPORTANT: Clone the response. A response is a stream
          // and because we want the browser to consume the response
          // as well as the cache consuming the response, we need
          // to clone it so we have two streams.
          var responseToCache = response.clone();

          caches.open(CACHE_NAME)
            .then(function (cache) {
              cache.put(event.request, responseToCache);
            });

          return response;
        }
      );
    })
  );
});

/*
self.addEventListener('fetch', function (event) {
// it can be empty if you just want to get rid of that error
});
*/