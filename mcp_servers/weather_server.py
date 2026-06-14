#!/usr/bin/env python3
"""
RainCppAI Weather MCP Server
使用 FastMCP 框架，提供 get_weather 工具。
通过 wttr.in 获取实时天气，无需 API Key。
"""

import requests
from mcp.server.fastmcp import FastMCP

mcp = FastMCP("weather_agent")


@mcp.tool()
def get_weather(city: str) -> dict:
    """
    获取指定城市的实时天气信息

    Args:
        city: 城市名称，如 北京、上海、广州
    """
    try:
        url = f"https://wttr.in/{city}?format=%l:+%C+%t+%w&lang=zh"
        resp = requests.get(url, timeout=8)
        resp.raise_for_status()
        return {"city": city, "weather": resp.text.strip(), "source": "wttr.in"}
    except Exception as e:
        return {
            "city": city,
            "weather": "当前网络无法获取实时天气，建议用户查看手机天气App",
            "source": "fallback",
            "error": str(e),
        }


if __name__ == "__main__":
    mcp.run(transport="stdio")
