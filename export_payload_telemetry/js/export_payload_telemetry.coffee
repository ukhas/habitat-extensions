db = $.couch.db("habitat")

show_telem = (data) ->
    for row in data.rows
        $('#output').append(row['doc']['data']['_raw'])

load_telem = (callsign) ->
    db.view "habitat/payload_telemetry", {
        startkey: [callsign, 0],
        endkey: [callsign, "end"],
        include_docs: true,
        stale: "update_after",
        success: show_telem
    }

$('#output').click (event) ->
    $('#output').focus()
    $('#output').select()

$('#input').submit (event) ->
    load_telem $('#callsign').val()
    return false

$('#callsign').focus()

