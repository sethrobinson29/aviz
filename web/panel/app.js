(function () {
    var RADIAL_MIN_RADIUS_GAP = 0.05;

    function roundToHundredths(value) {
        return Math.round(value * 100) / 100;
    }
    var BLEND_MODES = ["Normal", "Additive", "Multiply", "Screen"];
    var REACTIVITY_SOURCES = ["GlobalBeat", "BandOnset", "BandAmplitude"];
    var NUM_BANDS = 16;
    var LAYER_NAME_MAX_LENGTH = 50;
    var PALETTE_NONE = "None";
    var PALETTE_CUSTOM = "Custom";
    var NAMED_PALETTES = [
        {
            name: "Warm-Cool",
            stops: [
                { position: 0, color: { r: 40, g: 90, b: 255, a: 255 } },
                { position: 1, color: { r: 255, g: 60, b: 180, a: 255 } }
            ]
        },
        {
            name: "Sunset",
            stops: [
                { position: 0, color: { r: 255, g: 220, b: 0, a: 255 } },
                { position: 0.5, color: { r: 255, g: 80, b: 0, a: 255 } },
                { position: 1, color: { r: 120, g: 0, b: 200, a: 255 } }
            ]
        }
    ];

    var layerListEl = document.getElementById("layer-list");
    var statusEl = document.getElementById("status");
    var savePresetNameEl = document.getElementById("save-preset-name");
    var dockCheckbox = document.getElementById("dock-checkbox");
    var sourceButton = document.getElementById("source-button");
    var importFileButton = document.getElementById("import-file-button");
    var enableLoopbackButton = document.getElementById("enable-loopback-button");
    var iconTemplate = document.getElementById("icon-template");

    var SOURCE_BUTTON_LABELS = { none: "No Source", file: "File", loopback: "Loopback" };
    var SOURCE_BUTTON_TITLES = {
        none: "Import a file or enable loopback to choose an audio source",
        file: "Playing a file. Click to switch to loopback",
        loopback: "Capturing loopback. Click to switch back to the imported file"
    };
    var TEXT_ENTRY_INPUT_TYPES = ["text", "search", "email", "url", "password", "number", "tel"];

    var windowState = { docked: true, fullscreen: false, source: "none" };

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

    function stopsMatch(a, b) {
        return a.length === b.length && a.every(function (stop, i) {
            var other = b[i];
            return stop.position === other.position &&
                stop.color.r === other.color.r &&
                stop.color.g === other.color.g &&
                stop.color.b === other.color.b &&
                stop.color.a === other.color.a;
        });
    }

    function paletteSelectionFor(paletteOverride) {
        if (paletteOverride === null) {
            return PALETTE_NONE;
        }
        var match = NAMED_PALETTES.find(function (palette) {
            return stopsMatch(palette.stops, paletteOverride.stops);
        });
        return match ? match.name : PALETTE_CUSTOM;
    }

    var VISUAL_TYPE_INFO = {
        Waveform: { label: "Waveform", chip: "WAV" },
        Spectrum: { label: "Spectrum", chip: "SPC" },
        Radial: { label: "Radial", chip: "RAD" },
        Lissajous: { label: "Lissajous", chip: "LIS" },
        Particles: { label: "Particles", chip: "PRT" },
        FlowField: { label: "Flow Field", chip: "FLW" },
        Fractal: { label: "Fractal", chip: "FRC" }
    };

    var expandedModuleSections = new Map();

    function visualTypeInfo(visualType) {
        return VISUAL_TYPE_INFO[visualType] || { label: String(visualType), chip: "?" };
    }

    function stepDecimals(step) {
        var text = String(step);
        var dot = text.indexOf(".");
        return dot < 0 ? 0 : text.length - dot - 1;
    }

    function formatSliderValue(input, value) {
        var number = Number(value);
        if (!isFinite(number)) return "";
        var decimals = stepDecimals(input.step);
        var text = number.toFixed(decimals);
        if (parseFloat(text) === 0) {
            text = (0).toFixed(decimals);
        }
        if (parseFloat(input.min) < 0 && parseFloat(text) > 0) {
            text = "+" + text;
        }
        return text;
    }

    function setSliderFill(input) {
        var min = parseFloat(input.min);
        var max = parseFloat(input.max);
        var value = parseFloat(input.value);
        var fraction = max > min ? (value - min) / (max - min) : 0;
        fraction = Math.min(1, Math.max(0, fraction));
        input.style.setProperty("--p", String(fraction));
    }

    function iconSvg(symbolId) {
        var svg = iconTemplate.content.firstElementChild.cloneNode(true);
        svg.querySelector("use").setAttribute("href", "#" + symbolId);
        return svg;
    }

    function iconButton(symbolId, label, onClick) {
        var button = document.createElement("button");
        button.type = "button";
        button.className = "icon-button";
        button.title = label;
        button.setAttribute("aria-label", label);
        button.appendChild(iconSvg(symbolId));
        button.addEventListener("click", onClick);
        return button;
    }

    function fieldCaption(labelText) {
        var caption = document.createElement("span");
        caption.className = "field-caption";
        var text = document.createElement("span");
        text.className = "field-caption-text";
        text.textContent = labelText;
        caption.appendChild(text);
        return caption;
    }

    function selectField(labelText, select) {
        var label = document.createElement("label");
        label.className = "field";
        label.appendChild(fieldCaption(labelText));
        select.classList.add("pill");
        label.appendChild(select);
        return label;
    }

    function sliderField(labelText, min, max, step, onInput) {
        var label = document.createElement("label");
        label.className = "field slider-field";
        var caption = fieldCaption(labelText);
        var readout = document.createElement("span");
        readout.className = "field-value";
        caption.appendChild(readout);
        label.appendChild(caption);

        var input = document.createElement("input");
        input.type = "range";
        input.className = "track";
        input.min = min;
        input.max = max;
        input.step = step;
        input.addEventListener("input", function () {
            onInput(input);
            setSliderFill(input);
            readout.textContent = formatSliderValue(input, input.value);
        });
        label.appendChild(input);

        return { field: label, input: input, readout: readout };
    }

    function switchField(labelText, onChange) {
        var label = document.createElement("label");
        label.className = "field switch-field";
        label.appendChild(fieldCaption(labelText));
        var input = document.createElement("input");
        input.type = "checkbox";
        input.className = "switch";
        input.addEventListener("change", function () {
            onChange(input);
        });
        label.appendChild(input);
        return { field: label, input: input };
    }

    function syncSlider(slider, value, active) {
        if (active !== slider.input) {
            slider.input.value = value;
        }
        setSliderFill(slider.input);
        slider.readout.textContent = formatSliderValue(slider.input, value);
    }

    function controlRow(extraClass, fields) {
        var row = document.createElement("div");
        row.className = extraClass ? "control-row " + extraClass : "control-row";
        fields.forEach(function (field) {
            row.appendChild(field);
        });
        return row;
    }

    function buildLayerRow(layer) {
        var info = visualTypeInfo(layer.visual_type);

        var li = document.createElement("li");
        li.className = "layer-row";
        li.dataset.layerId = String(layer.layer_id);
        li.dataset.visualType = layer.visual_type;

        var header = document.createElement("div");
        header.className = "layer-header";

        var handle = document.createElement("span");
        handle.className = "drag-handle";
        handle.draggable = true;
        handle.title = "Drag to reorder";
        handle.setAttribute("aria-hidden", "true");
        header.appendChild(handle);

        var number = document.createElement("span");
        number.className = "layer-number";
        header.appendChild(number);

        var enabledInput = document.createElement("input");
        enabledInput.type = "checkbox";
        enabledInput.className = "switch";
        enabledInput.addEventListener("change", function () {
            post("set_enabled", { layer_id: layer.layer_id, enabled: enabledInput.checked });
        });
        header.appendChild(enabledInput);

        var nameInput = document.createElement("input");
        nameInput.type = "text";
        nameInput.className = "layer-name";
        nameInput.maxLength = LAYER_NAME_MAX_LENGTH;
        nameInput.setAttribute("aria-label", "Layer name");
        nameInput.addEventListener("change", function () {
            post("set_layer_name", { layer_id: layer.layer_id, name: nameInput.value });
        });
        nameInput.addEventListener("keydown", function (e) {
            if (e.key === "Enter") {
                nameInput.blur();
            }
        });
        header.appendChild(nameInput);

        var chip = document.createElement("abbr");
        chip.className = "type-chip";
        chip.title = info.label;
        chip.textContent = info.chip;
        header.appendChild(chip);

        li.appendChild(header);

        var reactivitySelect = document.createElement("select");
        populateSelect(reactivitySelect, REACTIVITY_SOURCES);
        reactivitySelect.addEventListener("change", function () {
            post("set_reactivity_source", { layer_id: layer.layer_id, reactivity_source: reactivitySelect.value });
        });
        li.appendChild(controlRow(null, [selectField("Reactivity", reactivitySelect)]));

        var blendSelect = document.createElement("select");
        populateSelect(blendSelect, BLEND_MODES);
        blendSelect.addEventListener("change", function () {
            post("set_blend_mode", { layer_id: layer.layer_id, blend_mode: blendSelect.value });
        });

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

        var paletteSelect = document.createElement("select");
        populateSelect(paletteSelect, [PALETTE_NONE].concat(NAMED_PALETTES.map(function (palette) {
            return palette.name;
        })));
        var paletteCustomOption = document.createElement("option");
        paletteCustomOption.value = PALETTE_CUSTOM;
        paletteCustomOption.textContent = PALETTE_CUSTOM;
        paletteCustomOption.disabled = true;
        paletteCustomOption.hidden = true;
        paletteSelect.appendChild(paletteCustomOption);
        paletteSelect.addEventListener("change", function () {
            var selected = NAMED_PALETTES.find(function (palette) {
                return palette.name === paletteSelect.value;
            });
            post("set_palette_override", {
                layer_id: layer.layer_id,
                palette_override: selected ? { stops: selected.stops } : null
            });
        });

        li.appendChild(controlRow(null, [
            selectField("Blend", blendSelect),
            selectField("Band", bandSelect),
            selectField("Palette", paletteSelect)
        ]));

        var opacitySlider = sliderField("Opacity", "0", "1", "0.01", function (input) {
            post("set_opacity", { layer_id: layer.layer_id, opacity: parseFloat(input.value) });
        });
        var sensitivitySlider = sliderField("Sensitivity", "0", "4", "0.1", function (input) {
            post("set_sensitivity", { layer_id: layer.layer_id, sensitivity: parseFloat(input.value) });
        });
        li.appendChild(controlRow("slider-row", [opacitySlider.field, sensitivitySlider.field]));

        var moduleControls = document.createElement("div");
        moduleControls.className = "module-controls";
        moduleControls.id = "module-controls-" + layer.layer_id;
        var visualParamSliders = [];
        var visualParamSwitches = [];

        function addVisualParamSlider(labelText, paramName, min, max, step, limit) {
            var slider = sliderField(labelText, min, max, step, function (input) {
                if (limit) {
                    input.value = limit(parseFloat(input.value));
                }
                post("set_visual_param", {
                    layer_id: layer.layer_id,
                    param_name: paramName,
                    value: parseFloat(input.value)
                });
            });
            moduleControls.appendChild(slider.field);
            slider.paramName = paramName;
            visualParamSliders.push(slider);
            return slider.input;
        }

        function addVisualParamSwitch(labelText, paramName) {
            var entry = switchField(labelText, function (input) {
                post("set_visual_param", {
                    layer_id: layer.layer_id,
                    param_name: paramName,
                    value: input.checked
                });
            });
            moduleControls.appendChild(entry.field);
            entry.paramName = paramName;
            visualParamSwitches.push(entry);
        }

        if (layer.visual_type === "Particles") {
            addVisualParamSlider("Spawn Rate", "spawn_rate_per_frame", "0", "20", "0.1");
            addVisualParamSlider("Draw Distance", "lifetime_frames", "10", "600", "10");
            addVisualParamSlider("Inner Radius", "inner_radius_fraction", "0", "1", "0.01");
        } else if (layer.visual_type === "FlowField") {
            addVisualParamSlider("Spawn Rate", "spawn_rate_per_frame", "0", "5", "0.05");
            addVisualParamSlider("Trail Life", "lifetime_frames", "10", "600", "10");
            addVisualParamSlider("Turbulence", "turbulence_frequency", "0.002", "0.05", "0.001");
            addVisualParamSlider("Evolution Speed", "evolution_per_frame", "0", "0.05", "0.001");
            addVisualParamSlider("Grid Resolution", "grid_cols", "8", "32", "1");
            addVisualParamSwitch("Ripple on Onset", "ripple_on_onset_enabled");
            addVisualParamSwitch("Burst on Onset", "burst_on_onset_enabled");
        } else if (layer.visual_type === "Radial") {
            var innerRadiusInput = addVisualParamSlider(
                "Inner Radius", "inner_radius_fraction", "0", "0.95", "0.01",
                function (value) {
                    return Math.min(value, roundToHundredths(parseFloat(outerRadiusInput.value) - RADIAL_MIN_RADIUS_GAP));
                });
            var outerRadiusInput = addVisualParamSlider(
                "Outer Radius", "max_radius_fraction", "0.05", "1", "0.01",
                function (value) {
                    return Math.max(value, roundToHundredths(parseFloat(innerRadiusInput.value) + RADIAL_MIN_RADIUS_GAP));
                });
        } else if (layer.visual_type === "Lissajous") {
            addVisualParamSlider("Curve Exponent", "compression_exponent", "0.2", "1", "0.05");
        } else if (layer.visual_type === "Fractal") {
            addVisualParamSlider("Zoom", "base_zoom", "1", "4", "0.05");
            addVisualParamSlider("Zoom Reactivity", "zoom_amplitude_scale", "0", "2", "0.05");
            addVisualParamSlider("Orbit Radius", "julia_base_radius", "0.7", "0.85", "0.005");
            addVisualParamSlider("Radius Reactivity", "julia_radius_amplitude_scale", "0", "0.2", "0.005");
            addVisualParamSlider("Orbit Speed", "julia_angular_speed_radians_per_sec", "0", "1", "0.01");
            addVisualParamSlider("Orbit Center X", "julia_orbit_center_x", "-0.5", "0.5", "0.01");
            addVisualParamSlider("Orbit Center Y", "julia_orbit_center_y", "-0.5", "0.5", "0.01");
            addVisualParamSlider("Color Cycle Speed", "color_cycle_speed_hz", "0", "0.5", "0.005");
            addVisualParamSwitch("Clockwise Orbit", "julia_orbit_clockwise");
        }

        var moduleToggle = null;
        if (moduleControls.childElementCount > 0) {
            var expanded = expandedModuleSections.get(layer.layer_id) === true;
            moduleControls.hidden = !expanded;
            moduleToggle = iconButton("icon-chevron", "", function () {
                var nowExpanded = moduleControls.hidden;
                moduleControls.hidden = !nowExpanded;
                if (nowExpanded) {
                    expandedModuleSections.set(layer.layer_id, true);
                } else {
                    expandedModuleSections.delete(layer.layer_id);
                }
                updateModuleToggle(moduleToggle, info, nowExpanded);
            });
            moduleToggle.classList.add("module-toggle");
            moduleToggle.setAttribute("aria-controls", moduleControls.id);
            updateModuleToggle(moduleToggle, info, expanded);
            header.appendChild(moduleToggle);
            li.appendChild(moduleControls);
        }

        header.appendChild(iconButton("icon-duplicate", "Duplicate " + info.label + " layer", function () {
            post("duplicate_layer", { layer_id: layer.layer_id });
        }));
        header.appendChild(iconButton("icon-remove", "Remove " + info.label + " layer", function () {
            post("remove_layer", { layer_id: layer.layer_id });
        }));

        li._els = {
            number: number,
            name: nameInput,
            enabled: enabledInput,
            opacity: opacitySlider,
            blend: blendSelect,
            palette: paletteSelect,
            paletteCustomOption: paletteCustomOption,
            band: bandSelect,
            reactivity: reactivitySelect,
            sensitivity: sensitivitySlider,
            visualParamSliders: visualParamSliders,
            visualParamSwitches: visualParamSwitches,
            info: info
        };

        return li;
    }

    function updateModuleToggle(toggle, info, expanded) {
        var label = (expanded ? "Hide " : "Show ") + info.label + " controls";
        toggle.title = label;
        toggle.setAttribute("aria-label", label);
        toggle.setAttribute("aria-expanded", expanded ? "true" : "false");
    }

    function updateLayerRow(li, layer, index) {
        var els = li._els;
        var position = index + 1;
        els.number.textContent = position < 10 ? "0" + position : String(position);
        li.classList.toggle("is-disabled", !layer.enabled);
        els.enabled.setAttribute("aria-label", els.info.label + " layer visible");

        var active = document.activeElement;
        els.name.placeholder = "Layer " + position;
        if (active !== els.name) els.name.value = layer.name;
        if (active !== els.enabled) els.enabled.checked = layer.enabled;
        syncSlider(els.opacity, layer.opacity, active);
        if (active !== els.blend) els.blend.value = layer.blend_mode;
        var paletteSelection = paletteSelectionFor(layer.palette_override);
        els.paletteCustomOption.hidden = paletteSelection !== PALETTE_CUSTOM;
        if (active !== els.palette) els.palette.value = paletteSelection;
        els.visualParamSliders.forEach(function (slider) {
            syncSlider(slider, layer.params[slider.paramName], active);
        });
        els.visualParamSwitches.forEach(function (entry) {
            if (active !== entry.input) entry.input.checked = layer.params[entry.paramName] === true;
        });
        var bandValue = layer.freq_band === null ? "Global" : String(layer.freq_band);
        if (active !== els.band) els.band.value = bandValue;
        if (active !== els.reactivity) els.reactivity.value = layer.reactivity_source;
        syncSlider(els.sensitivity, layer.sensitivity, active);
    }

    function renderLayerList(layers) {
        var listScrollTop = layerListEl.scrollTop;

        var seenIds = new Set();
        var currentOrder = Array.prototype.map.call(layerListEl.children, function (el) {
            return el.dataset.layerId;
        });
        var desiredOrder = layers.map(function (layer) { return String(layer.layer_id); });
        var orderChanged = currentOrder.length !== desiredOrder.length ||
            currentOrder.some(function (id, i) { return id !== desiredOrder[i]; });

        layers.forEach(function (layer, index) {
            var li = layerRowElements.get(layer.layer_id);
            if (!li || li.dataset.visualType !== layer.visual_type) {
                if (li) {
                    expandedModuleSections.delete(layer.layer_id);
                }
                var rebuilt = buildLayerRow(layer);
                if (li) {
                    li.replaceWith(rebuilt);
                }
                li = rebuilt;
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
                expandedModuleSections.delete(id);
            }
        });

        layerListEl.scrollTop = listScrollTop;
    }

    function showStatus(ok, message) {
        statusEl.textContent = message;
        statusEl.title = message;
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
        } else if (data.kind === "status") {
            showStatus(data.ok, data.message);
        } else if (data.kind === "window_state") {
            renderWindowState(data);
        }
    });

    function renderWindowState(data) {
        var source = SOURCE_BUTTON_LABELS.hasOwnProperty(data.source) ? data.source : "none";
        windowState = { docked: data.docked === true, fullscreen: data.fullscreen === true, source: source };
        dockCheckbox.checked = windowState.docked;
        sourceButton.textContent = SOURCE_BUTTON_LABELS[source];
        sourceButton.title = SOURCE_BUTTON_TITLES[source];
        sourceButton.disabled = source === "none";
        enableLoopbackButton.disabled = source === "loopback";
        enableLoopbackButton.title = source === "loopback" ? "Loopback capture is already active" : "";
    }

    function isTextEntry(el) {
        if (!el) return false;
        if (el.isContentEditable || el.tagName === "TEXTAREA") return true;
        return el.tagName === "INPUT" && TEXT_ENTRY_INPUT_TYPES.indexOf(el.type) !== -1;
    }

    dockCheckbox.addEventListener("change", function () {
        post("set_docked", { docked: dockCheckbox.checked });
    });

    sourceButton.addEventListener("click", function () {
        if (sourceButton.disabled) return;
        post("switch_source", {});
    });

    importFileButton.addEventListener("click", function () {
        post("open_file_dialog", {});
    });

    enableLoopbackButton.addEventListener("click", function () {
        if (enableLoopbackButton.disabled) return;
        post("enable_loopback", {});
    });

    document.addEventListener("keydown", function (e) {
        if (isTextEntry(document.activeElement)) return;
        if (e.repeat || e.ctrlKey || e.altKey || e.metaKey) return;

        var key = e.key.toLowerCase();
        if (e.key === "F11") {
            e.preventDefault();
            post("set_fullscreen", { fullscreen: !windowState.fullscreen });
        } else if (key === "k") {
            post("set_docked", { docked: !windowState.docked });
        } else if (key === "l") {
            if (windowState.source !== "none") post("switch_source", {});
        }
    });

    function isPrimaryPress(e) {
        return e.button === 0 && !e.ctrlKey && !e.altKey && !e.metaKey && !e.shiftKey;
    }

    document.querySelector(".title-bar").addEventListener("mousedown", function (e) {
        if (!isPrimaryPress(e)) return;
        if (e.target.closest("button, input, select")) return;
        e.preventDefault();
        post("window_drag", { region: "caption" });
    });

    document.getElementById("window-close-button").addEventListener("click", function () {
        post("window_command", { command: "close" });
    });

    document.getElementById("window-minimize-button").addEventListener("click", function () {
        post("window_command", { command: "minimize" });
    });

    Array.prototype.forEach.call(document.querySelectorAll(".window-resize"), function (handle) {
        handle.addEventListener("mousedown", function (e) {
            if (!isPrimaryPress(e)) return;
            e.preventDefault();
            post("window_drag", { region: handle.dataset.windowRegion });
        });
    });

    Array.prototype.forEach.call(document.querySelectorAll(".add-button"), function (button) {
        button.addEventListener("click", function () {
            post("add_layer", { visual_type: button.dataset.visualType });
        });
    });

    document.getElementById("load-preset-button").addEventListener("click", function () {
        post("open_preset_dialog", {});
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
