R"EOC(
<head>
	<title>Universal RGB</title>
	<link href="https://cdn.jsdelivr.net/npm/halfmoon@1.1.1/css/halfmoon.min.css" rel="stylesheet" />
</head>
<body class="dark-mode">
	<div class="container-lg">
		<div class="content">
			<h1>Universal RGB</h1>
			<p id="status-text">No effect is currently active.</p>
			<div id="preview" style="margin-bottom:1em"></div>
			<hr>
			<h3>Inputs</h3>
			<div class="custom-checkbox mb-20"><input type="checkbox" id="chroma-checkbox" onchange="updateInput(this);" /><label for="chroma-checkbox">Razer Chroma</label></div>
			<div class="custom-checkbox mb-20"><input type="checkbox" id="logitech-checkbox" onchange="updateInput(this);" /><label for="logitech-checkbox">Logitech Liaison</label></div>
			<hr>
			<h3>Outputs</h3>
			<ul id="outputs"></ul>
			<p>
				<button class="btn btn-primary" onclick="refreshOutputsNoChroma();">Refresh</button>
				<button class="btn btn-default" onclick="refreshOutputs();">Refresh (Including Chroma)</button> <!-- Weird issue with Warframe where if we use the Chroma API, it fails to init Chroma, so not outputting to Chroma by default. -->
			</p>
			<hr>
			<p><a class="btn btn-danger" href="/api/exit" target="_blank">Exit Universal RGB</a></p>
		</div>
	</div>
	<script>
		function showEffectLoop()
		{
			fetch("/api/effect").then(res => res.text()).then(html => {
				if (html.length == 0)
				{
					document.getElementById("status-text").textContent = "No effect is currently active.";
					document.getElementById("preview").innerHTML = "";
				}
				else
				{
					document.getElementById("status-text").textContent = "";
					document.getElementById("preview").innerHTML = html;
				}
				if (location.hash != "#noeffectloop")
				{
					showEffectLoop();
				}
			}).catch(function()
			{
				document.getElementById("status-text").textContent = "Connection to Universal RGB has been lost. Refresh the page to attempt to re-establish.";
				document.getElementById("preview").innerHTML = "";
			});
		}
		showEffectLoop();

		function showOutputs(promise)
		{
			promise.then(res => res.text()).then(data => {
				if (data.length == 0)
				{
					data = "No supported SDK or device detected.";
				}
				document.getElementById("outputs").innerHTML = "";
				data.split("%").forEach(out => {
					let li = document.createElement("li");
					li.textContent = out;
					document.getElementById("outputs").appendChild(li);
				});
			});
		}
		showOutputs(fetch("/api/outputs"));

		function refreshOutputs()
		{
			showOutputs(fetch("/api/refresh-outputs"));
		}

		function refreshOutputsNoChroma()
		{
			showOutputs(fetch("/api/refresh-outputs-nochroma"));
		}

		fetch("/api/inputs").then(res => res.text()).then(data => {
			data.split("%").forEach(input => {
				document.getElementById(input + "-checkbox").checked = true;
			});
		});

		function updateInput(elm)
		{
			let input_name = elm.id.substr(0, elm.id.length - 9); // "-checkbox"
			fetch("/api/" + (elm.checked ? "enable" : "disable") + "-input-" + input_name);
		}
	</script>
</body>
)EOC"