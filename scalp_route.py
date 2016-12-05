"""
set of scalps for the log component
"""

from scalp_frame import Scalp


class RouteList(Scalp):
    """
    number of set routes
    argv #0 response : number of set routes
    """


class RouteLine(Scalp):
    """
    retrieve a line content
    argv #0 request : requested line
    argv #1 response : virtual address
    argv #2 response : routed address
    argv #3 response : result OK (1) or ko (0)
    """


class RouteAdd(Scalp):
    """
    add a new route
    argv #0 request : virtual address
    argv #1 request : routed address
    argv #2 response : result OK (1) or ko (0)
    """


class RouteDel(Scalp):
    """
    delete a route
    argv #0 request : virtual address
    argv #1 request : routed address
    argv #2 response : result OK (1) or ko (0)
    """

