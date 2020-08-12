/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    realm.ck

Abstract:

    This module implements support for realms within Santa.

Author:

    Evan Green 25-May-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from santa.file import cptree, link, path, rmtree;
from santa.lib.santaconfig import SANTA_GLOBAL_CONFIG_PATH, SANTA_STORAGE_PATH;
from santa.modules import getContainment, getPresentation;

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

class Realm {
    var _parameters;

    function
    create (
        parameters
        )

    /*++

    Routine Description:

        This routine creates a new Realm.

    Arguments:

        parameters - Supplies the realm creation parameters.

    Return Value:

        Null on success.

        Raises an exception on failure.

    --*/

    {

        var type;

        _parameters = parameters;

        //
        // Create the container, storage, and presentation objects based on
        // their configured types.
        //

        type = getContainment(_parameters.containment.type);
        this.containment = type();
        this.containment.create(parameters.containment);
        type = getPresentation(parameters.presentation.type);
        this.presentation = type();
        this.presentation.create(parameters.presentation);
        return null;
    }

    function
    destroy (
        )

    /*++

    Routine Description:

        This routine removes a realm from disk.

    Arguments:

        None.

    Return Value:

        None.

    --*/

    {

        this.presentation.destroy();
        this.presentation = null;
        this.containment.destroy();
        this.containment = null;
        _parameters = null;
        return;
    }

    function
    load (
        parameters
        )

    /*++

    Routine Description:

        This routine loads a Realm from previously saved parameters.

    Arguments:

        parameters - Supplies the realm's saved parameters.

    Return Value:

        Null on success.

        Raises an exception on failure.

    --*/

    {

        var type;

        _parameters = parameters;

        //
        // Create the container, storage, and presentation objects based on
        // their configured types.
        //

        type = getContainment(_parameters.containment.type);
        this.containment = type();
        this.containment.load(parameters.containment);
        type = getPresentation(parameters.presentation.type);
        this.presentation = type();
        this.presentation.load(parameters.presentation);
        return;
    }

    function
    save (
        )

    /*++

    Routine Description:

        This routine returns a dictionary representing the realm's current
        state.

    Arguments:

        None.

    Return Value:

        Returns the current dictionary of the realm's state on success.

        Raises an exception on failure.

    --*/

    {

        _parameters.containment = this.containment.save();
        _parameters.presentation = this.presentation.save();
        return _parameters;
    }

    function
    _shareFile (
        source,
        destination,
        method
        )

    /*++

    Routine Description:

        This routine creates a shared file.

    Arguments:

        source - Supplies the source path to share with the child realm.

        destination - Supplies the relative destination within the child to
            share the source at.

        method - Supplies the sharing method.

    Return Value:

        Null on success.

        Raises an exception on failure.

    --*/

    {

        destination = this.containment.outerPath(destination);
        if (method == "hardlink") {
            link(source, destination);

        } else if (method == "copy") {
            cptree(source, destination);

        } else if (method != "none") {
            Core.raise(ValueError("Unknown share method '%s'" % method));
        }

        return;
    }

    function
    _unshareFile (
        destination,
        method
        )

    /*++

    Routine Description:

        This routine removes a shared file.

    Arguments:

        destination - Supplies the relative destination within the child where
            the file was shared to.

        method - Supplies the sharing method.

    Return Value:

        Null on success.

        Raises an exception on failure.

    --*/

    {

        destination = this.containment.outerPath(destination);
        if ((method == "hardlink") || (method == "copy")) {
            rmtree(destination);

        } else {
            Core.raise(ValueError("Unknown share method '%s'" % method));
        }

        return;
    }
}

//
// --------------------------------------------------------- Internal Functions
//

