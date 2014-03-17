// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The include directives are put into Javascript-style comments to prevent
// parsing errors in non-flattened mode. The flattener still sees them.
// Note that this makes the flattener to comment out the first line of the
// included file but that's all right since any javascript file should start
// with a copyright comment anyway.

//<include src="../../../../../../../ui/webui/resources/js/cr.js"/>
//<include src="../../../../../../../ui/webui/resources/js/cr/event_target.js"/>
//<include src="../../../../../../../ui/webui/resources/js/cr/ui/array_data_model.js"/>
//<include src="../../../../../../../ui/webui/resources/js/load_time_data.js"/>

(function() {
// 'strict mode' is invoked for this scope.

//<include src="../../../common/js/async_util.js"/>
//<include src="../../../common/js/util.js"/>
//<include src="../../../common/js/path_util.js"/>
//<include src="../file_type.js"/>
//<include src="../volume_manager_wrapper.js">
//<include src="../metadata/metadata_cache.js"/>

//<include src="media_controls.js"/>
//<include src="util.js"/>
//<include src="video_player.js"/>
//<include src="player_testapi.js"/>

window.reload = reload;
window.unload = unload;

})();
