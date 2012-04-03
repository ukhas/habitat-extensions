db = $.couch.db("habitat")

show_telem = (data) ->
    $('#output').html ""
    for row in data.rows
        $('#output').append $.base64.decode row['doc']['data']['_raw']

load_telem = ->
    $('#output').html "Loading (may take some time)..."
    callsign = $('#callsign').val()
    start = $('#start').val()
    end = $('#end').val()

    if start
        start = new Date(start)
        start = start.getTime() / 1000
    else
        start = 0

    if end
        end = new Date(end)
        end = end.getTime() / 1000
    else
        end = "end"

    db.view "habitat/payload_telemetry", {
        startkey: [callsign, start],
        endkey: [callsign, end],
        include_docs: true,
        stale: "update_after",
        success: show_telem
    }

$('.datepicker').datepicker {dateFormat: "yy-mm-dd"}

$('#output').click (event) ->
    $('#output').focus()
    $('#output').select()

$('#input').submit (event) ->
    event.preventDefault()
    load_telem()

$('#callsign').focus()

