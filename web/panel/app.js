(function () {
    var VISUAL_TYPES = ["Waveform", "Spectrum", "Radial", "Particles", "Lissajous", "FlowField", "Fractal"];
    var BLEND_MODES = ["Normal", "Additive", "Multiply", "Screen"];
    var REACTIVITY_SOURCES = ["GlobalBeat", "BandOnset", "BandAmplitude"];
    var NUM_BANDS = 16;

    var layerListEl = document.getElementById("layer-list");
    var presetSelectEl = document.getElementById("preset-select");
    var addVisualTypeEl = document.getElementById("add-visual-type");
    var statusEl = document.getElementById("status");
    var savePresetNameEl = document.getElementById("save-preset-name");

    var layerRowElements = new Map();

    function post(type, fields) {
        var msg = Object.assign({ type: type }, fields || {});
        window.chrome.webview.postMessage(JSON.stringify(msg));
    }

    function populateSelect(select, options) {
        options.forEach(function (opt) {
            var el = document.createElement("option");
            el.value = opt;
            el.textContent = opt;
            select.appendChild(el);
        });
    }
    populateSelect(addVisualTypeEl, VISUAL_TYPES);

    function labeledControl(labelText, inputEl) {
        var label = document.createElement("label");
        var span = document.createElement("span");
        span.textContent = labelText;
        label.appendChild(span);
        label.appendChild(inputEl);
        return label;
    }

    function buildLayerRow(layer) {
        var li = document.createElement("li");
        li.className = "layer-row";
        li.dataset.layerId = String(layer.layer_id);

        var title = document.createElement("div");
        title.className = "layer-title";

        var handle = document.createElement("span");
        handle.className = "drag-handle";
        handle.draggable = true;
        handle.textContent = "⠷";

        var titleText = document.createElement("span");
        title.appendChild(handle);
        title.appendChild(titleText);
        li.appendChild(title);

        var controls = document.createElement("div");
        controls.className = "layer-controls";

        var enabledInput = document.createElement("input");
        enabledInput.type = "checkbox";
        enabledInput.addEventListener("change", function () {
            post("set_enabled", { layer_id: layer.layer_id, enabled: enabledInput.checked });
        });
        controls.appendChild(labeledControl("Enabled", enabledInput));

        var opacityInput = document.createElement("input");
        opacityInput.type = "range";
        opacityInput.min = "0";
        opacityInput.max = "1";
        opacityInput.step = "0.01";
        opacityInput.addEventListener("input", function () {
            post("set_opacity", { layer_id: layer.layer_id, opacity: parseFloat(opacityInput.value) });
        });
        controls.appendChild(labeledControl("Opacity", opacityInput));

        var blendSelect = document.createElement("select");
        populateSelect(blendSelect, BLEND_MODES);
        blendSelect.addEventListener("change", function () {
            post("set_blend_mode", { layer_id: layer.layer_id, blend_mode: blendSelect.value });
        });
        controls.appendChild(labeledControl("Blend", blendSelect));

        var bandSelect = document.createElement("select");
        var bandOptions = ["Global"];
        for (var b = 0; b < NUM_BANDS; b++) {
            bandOptions.push(String(b));
        }
        populateSelect(bandSelect, bandOptions);
        bandSelect.addEventListener("change", function () {
            var value = bandSelect.value === "Global" ? null : parseInt(bandSelect.value, 10);
            post("set_freq_band", { layer_id: layer.layer_id, freq_band: value });
        });
        controls.appendChild(labeledControl("Band", bandSelect));

        var reactivitySelect = document.createElement("select");
        populateSelect(reactivitySelect, REACTIVITY_SOURCES);
        reactivitySelect.addEventListener("change", function () {
            post("set_reactivity_source", { layer_id: layer.layer_id, reactivity_source: reactivitySelect.value });
        });
        controls.appendChild(labeledControl("Reactivity", reactivitySelect));

        var sensitivityInput = document.createElement("input");
        sensitivityInput.type = "range";
        sensitivityInput.min = "0";
        sensitivityInput.max = "4";
        sensitivityInput.step = "0.1";
        sensitivityInput.addEventListener("input", function () {
            post("set_sensitivity", { layer_id: layer.layer_id, sensitivity: parseFloat(sensitivityInput.value) });
        });
        controls.appendChild(labeledControl("Sensitivity", sensitivityInput));

        var dupBtn = document.createElement("button");
        dupBtn.type = "button";
        dupBtn.textContent = "Duplicate";
        dupBtn.addEventListener("click", function () {
            post("duplicate_layer", { layer_id: layer.layer_id });
        });
        controls.appendChild(dupBtn);

        var removeBtn = document.createElement("button");
        removeBtn.type = "button";
        removeBtn.textContent = "Remove";
        removeBtn.addEventListener("click", function () {
            post("remove_layer", { layer_id: layer.layer_id });
        });
        controls.appendChild(removeBtn);

        li.appendChild(controls);

        li._els = {
            title: titleText,
            enabled: enabledInput,
            opacity: opacityInput,
            blend: blendSelect,
            band: bandSelect,
            reactivity: reactivitySelect,
            sensitivity: sensitivityInput
        };

        return li;
    }

    function updateLayerRow(li, layer, index) {
        var els = li._els;
        els.title.textContent = index + ": " + layer.visual_type + " (#" + layer.layer_id + ")";

        var active = document.activeElement;
        if (active !== els.enabled) els.enabled.checked = layer.enabled;
        if (active !== els.opacity) els.opacity.value = layer.opacity;
        if (active !== els.blend) els.blend.value = layer.blend_mode;
        var bandValue = layer.freq_band === null ? "Global" : String(layer.freq_band);
        if (active !== els.band) els.band.value = bandValue;
        if (active !== els.reactivity) els.reactivity.value = layer.reactivity_source;
        if (active !== els.sensitivity) els.sensitivity.value = layer.sensitivity;
    }

    function renderLayerList(layers) {
        var scrollX = window.scrollX;
        var scrollY = window.scrollY;

        var seenIds = new Set();
        var currentOrder = Array.prototype.map.call(layerListEl.children, function (el) {
            return el.dataset.layerId;
        });
        var desiredOrder = layers.map(function (layer) { return String(layer.layer_id); });
        var orderChanged = currentOrder.length !== desiredOrder.length ||
            currentOrder.some(function (id, i) { return id !== desiredOrder[i]; });

        layers.forEach(function (layer, index) {
            var li = layerRowElements.get(layer.layer_id);
            if (!li) {
                li = buildLayerRow(layer);
                layerRowElements.set(layer.layer_id, li);
            }
            updateLayerRow(li, layer, index);
            seenIds.add(layer.layer_id);
            if (orderChanged) {
                layerListEl.appendChild(li);
            }
        });
        layerRowElements.forEach(function (li, id) {
            if (!seenIds.has(id)) {
                li.remove();
                layerRowElements.delete(id);
            }
        });

        window.scrollTo(scrollX, scrollY);
    }

    function renderPresetList(files) {
        var previousValue = presetSelectEl.value;
        presetSelectEl.textContent = "";
        files.forEach(function (name) {
            var opt = document.createElement("option");
            opt.value = name;
            opt.textContent = name;
            presetSelectEl.appendChild(opt);
        });
        if (files.indexOf(previousValue) !== -1) {
            presetSelectEl.value = previousValue;
        }
    }

    function showStatus(ok, message) {
        statusEl.textContent = message;
        statusEl.classList.toggle("error", !ok);
    }

    window.chrome.webview.addEventListener("message", function (event) {
        var data;
        try {
            data = JSON.parse(event.data);
        } catch (e) {
            return;
        }
        if (!data || typeof data.kind !== "string") {
            return;
        }
        if (data.kind === "layer_stack") {
            renderLayerList(data.state.layers);
        } else if (data.kind === "preset_list") {
            renderPresetList(data.files);
        } else if (data.kind === "status") {
            showStatus(data.ok, data.message);
        }
    });

    document.getElementById("add-layer-button").addEventListener("click", function () {
        post("add_layer", { visual_type: addVisualTypeEl.value });
    });

    document.getElementById("load-preset-button").addEventListener("click", function () {
        if (!presetSelectEl.value) return;
        post("load_preset", { filename: presetSelectEl.value });
    });

    document.getElementById("refresh-presets-button").addEventListener("click", function () {
        post("request_state", {});
    });

    document.getElementById("save-preset-button").addEventListener("click", function () {
        var name = savePresetNameEl.value.trim();
        if (!name) return;
        if (!/\.json$/i.test(name)) {
            name += ".json";
        }
        post("save_preset", { filename: name });
    });

    var draggedId = null;

    layerListEl.addEventListener("dragstart", function (e) {
        if (!e.target.closest(".drag-handle")) {
            e.preventDefault();
            return;
        }
        var row = e.target.closest(".layer-row");
        if (!row) return;
        draggedId = row.dataset.layerId;
        e.dataTransfer.effectAllowed = "move";
    });

    layerListEl.addEventListener("dragover", function (e) {
        var row = e.target.closest(".layer-row");
        if (!row) return;
        e.preventDefault();
        row.classList.add("drag-over");
    });

    layerListEl.addEventListener("dragleave", function (e) {
        var row = e.target.closest(".layer-row");
        if (row) row.classList.remove("drag-over");
    });

    layerListEl.addEventListener("drop", function (e) {
        e.preventDefault();
        var row = e.target.closest(".layer-row");
        Array.prototype.forEach.call(layerListEl.querySelectorAll(".layer-row"), function (r) {
            r.classList.remove("drag-over");
        });
        if (!row || draggedId === null) return;
        var targetIndex = Array.prototype.indexOf.call(layerListEl.children, row);
        post("move_layer", { layer_id: parseInt(draggedId, 10), to_index: targetIndex });
        draggedId = null;
    });

    post("request_state", {});
})();
