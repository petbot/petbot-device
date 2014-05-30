<html>
<head>
	<script src="js/jquery.js"></script>
	<script src="js/handlebars.js"></script>
	<script type="text/javascript">
		

		function get_networks(){
			$.get("ap_list.php", function(data){
				var networks_template = Handlebars.compile($("#networks_template").html())
				$("#network_list").html(networks_template({networks:data}))
                        }, "json");
		}
		
		$(document).ready(function(){
			get_networks()
		});

	</script>
</head>

<body>

<form action="wifi_connect.php" method="post" >
	<button type="button" onclick="get_networks()">refresh</button><br />
	<input type="password" name="password" /><br />
	<input type="submit" /><br />
	<div id="network_list"></div>
</form>
<script id="networks_template" type="text/x-handlebars-template">
	{{#each networks}}
		<input type="radio" name="ssid" value="{{this}}"/>{{this}}<br />
	{{/each}}
</script>

</body>

</html>
