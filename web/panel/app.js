(function () {
    var button = document.getElementById("add-layer-test");
    var status = document.getElementById("status");

    button.addEventListener("click", function () {
        window.chrome.webview.postMessage(JSON.stringify({ type: "add_layer" }));
        status.textContent = "sent add_layer message";
    });
})();
