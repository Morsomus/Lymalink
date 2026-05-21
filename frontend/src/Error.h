/////////////////////////////////////////////////////////
// File: Error.h
// Date: 2026-05-17
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Error
/////////////////////////////////////////////////////////

#pragma once

enum class Error {
    NoError,
    DatabaseError,
    FileSystemError,
    InvalidParameter,
    NoData,
    NotFound,
    Nullptr,
    ParseError
};
