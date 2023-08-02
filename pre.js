
// for nodejs

if (typeof XMLHttpRequest === 'undefined') {
  console.log("defining XMLHttpRequest");
  XMLHttpRequest = require('xhr2');
}

