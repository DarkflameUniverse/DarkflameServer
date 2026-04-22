/**
 * API Client for DarkflameServer Dashboard
 * Provides a simple interface for making API calls with error handling
 */

const API = {
    /**
     * Base URL for API endpoints
     */
    baseURL: '',

    /**
     * Make a GET request
     * @param {string} endpoint - The API endpoint
     * @param {object} params - Query parameters
     * @returns {Promise<any>} Response data
     */
    async get(endpoint, params = {}) {
        const url = new URL(this.baseURL + endpoint, window.location.origin);
        Object.keys(params).forEach(key => url.searchParams.append(key, params[key]));

        const response = await fetch(url, {
            method: 'GET',
            credentials: 'same-origin',
            headers: {
                'Accept': 'application/json'
            }
        });

        return this.handleResponse(response);
    },

    /**
     * Make a POST request
     * @param {string} endpoint - The API endpoint
     * @param {object} data - Request body data
     * @returns {Promise<any>} Response data
     */
    async post(endpoint, data = {}) {
        const response = await fetch(this.baseURL + endpoint, {
            method: 'POST',
            credentials: 'same-origin',
            headers: {
                'Content-Type': 'application/json',
                'Accept': 'application/json'
            },
            body: JSON.stringify(data)
        });

        return this.handleResponse(response);
    },

    /**
     * Make a PUT request
     * @param {string} endpoint - The API endpoint
     * @param {object} data - Request body data
     * @returns {Promise<any>} Response data
     */
    async put(endpoint, data = {}) {
        const response = await fetch(this.baseURL + endpoint, {
            method: 'PUT',
            credentials: 'same-origin',
            headers: {
                'Content-Type': 'application/json',
                'Accept': 'application/json'
            },
            body: JSON.stringify(data)
        });

        return this.handleResponse(response);
    },

    /**
     * Make a DELETE request
     * @param {string} endpoint - The API endpoint
     * @returns {Promise<any>} Response data
     */
    async delete(endpoint) {
        const response = await fetch(this.baseURL + endpoint, {
            method: 'DELETE',
            credentials: 'same-origin',
            headers: {
                'Accept': 'application/json'
            }
        });

        return this.handleResponse(response);
    },

    /**
     * Handle fetch response
     * @param {Response} response - Fetch response object
     * @returns {Promise<any>} Parsed response data
     */
    async handleResponse(response) {
        const contentType = response.headers.get('content-type');

        // Try to parse as JSON first (even if content-type is missing)
        try {
            const text = await response.text();

            // Try to parse as JSON
            if (text) {
                try {
                    const data = JSON.parse(text);

                    if (!response.ok) {
                        throw new Error(data.error || `HTTP error! status: ${response.status}`);
                    }

                    return data;
                } catch (jsonError) {
                    // Not JSON, return as text
                    if (!response.ok) {
                        throw new Error(`HTTP error! status: ${response.status}`);
                    }
                    return text;
                }
            }

            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }

            return text;
        } catch (error) {
            throw error;
        }
    }
};

/**
 * Logout function
 */
async function logout() {
    try {
        await API.post('/api/logout');
        window.location.href = '/login';
    } catch (error) {
        console.error('Logout error:', error);
        // Force redirect even on error
        window.location.href = '/login';
    }
}
